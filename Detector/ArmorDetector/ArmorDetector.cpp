// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ArmorDetector.hpp"
#include "core/hal/interface.h"
#include <cmath>

using namespace armor_detector;

// static constexpr int INPUT_W = 640;    // Width of input
// static constexpr int INPUT_H = 384;    // Height of input
static constexpr int INPUT_W = 416;   // Width of input
static constexpr int INPUT_H = 416;   // Height of input
static constexpr int NUM_CLASSES = 8; // Number of classes
static constexpr int NUM_COLORS = 4;  // Number of color
static constexpr int TOPK = 128;      // TopK
static constexpr float NMS_THRESH = 0.3;
static constexpr float BBOX_CONF_THRESH = 0.6;
static constexpr float FFT_CONF_ERROR = 0.15;
static constexpr float FFT_MIN_IOU = 0.9;

static inline int argmax(const float *ptr, int len)
{
    int max_arg = 0;
    for (int i = 1; i < len; i++)
    {
        if (ptr[i] > ptr[max_arg])
            max_arg = i;
    }
    return max_arg;
}

/**
 * @brief Resize the image using letterbox
 * @param img Image before resize
 * @param transform_matrix Transform Matrix of Resize
 * @return Image after resize
 */
inline cv::Mat scaledResize(cv::Mat &img, Eigen::Matrix<float, 3, 3> &transform_matrix)
{
    float r = std::min(INPUT_W / (img.cols * 1.0), INPUT_H / (img.rows * 1.0));
    int unpad_w = r * img.cols;
    int unpad_h = r * img.rows;

    int dw = INPUT_W - unpad_w;
    int dh = INPUT_H - unpad_h;

    dw /= 2;
    dh /= 2;

    transform_matrix << 1.0 / r, 0, -dw / r, 0, 1.0 / r, -dh / r, 0, 0, 1;

    Mat re;
    cv::resize(img, re, Size(unpad_w, unpad_h));
    Mat out;
    cv::copyMakeBorder(re, out, dh, dh, dw, dw, BORDER_CONSTANT);

    return out;
}

/**
 * @brief Generate grids and stride.
 * @param target_w Width of input.
 * @param target_h Height of input.
 * @param strides A vector of stride.
 * @param grid_strides Grid stride generated in this function.
 */
static void generate_grids_and_stride(const int target_w, const int target_h, std::vector<int> &strides,
                                      std::vector<GridAndStride> &grid_strides)
{
    for (auto stride : strides)
    {
        int num_grid_w = target_w / stride;
        int num_grid_h = target_h / stride;

        for (int g1 = 0; g1 < num_grid_h; g1++)
        {
            for (int g0 = 0; g0 < num_grid_w; g0++)
            {
                grid_strides.push_back((GridAndStride){g0, g1, stride});
            }
        }
    }
}

/**
 * @brief Generate Proposal
 * @param grid_strides Grid strides
 * @param feat_ptr Original predition result.
 * @param prob_threshold Confidence Threshold.
 * @param objects Objects proposed.
 */
static void generateYoloxProposals(std::vector<GridAndStride> grid_strides, const float *feat_ptr,
                                   Eigen::Matrix<float, 3, 3> &transform_matrix, float prob_threshold,
                                   std::vector<ArmorObject> &objects)
{

    const int num_anchors = grid_strides.size();
    // Travel all the anchors
    for (int anchor_idx = 0; anchor_idx < num_anchors; anchor_idx++)
    {
        const int grid0 = grid_strides[anchor_idx].grid0;
        const int grid1 = grid_strides[anchor_idx].grid1;
        const int stride = grid_strides[anchor_idx].stride;

        const int basic_pos = anchor_idx * (9 + NUM_COLORS + NUM_CLASSES);

        // yolox/models/yolo_head.py decode logic
        //  outputs[..., :2] = (outputs[..., :2] + grids) * strides
        //  outputs[..., 2:4] = torch.exp(outputs[..., 2:4]) * strides
        float x_1 = (feat_ptr[basic_pos + 0] + grid0) * stride;
        float y_1 = (feat_ptr[basic_pos + 1] + grid1) * stride;
        float x_2 = (feat_ptr[basic_pos + 2] + grid0) * stride;
        float y_2 = (feat_ptr[basic_pos + 3] + grid1) * stride;
        float x_3 = (feat_ptr[basic_pos + 4] + grid0) * stride;
        float y_3 = (feat_ptr[basic_pos + 5] + grid1) * stride;
        float x_4 = (feat_ptr[basic_pos + 6] + grid0) * stride;
        float y_4 = (feat_ptr[basic_pos + 7] + grid1) * stride;

        int box_color = argmax(feat_ptr + basic_pos + 9, NUM_COLORS);
        int box_class = argmax(feat_ptr + basic_pos + 9 + NUM_COLORS, NUM_CLASSES);

        float box_objectness = (feat_ptr[basic_pos + 8]);

        float color_conf = (feat_ptr[basic_pos + 9 + box_color]);
        float cls_conf = (feat_ptr[basic_pos + 9 + NUM_COLORS + box_class]);

        // float box_prob = (box_objectness + cls_conf + color_conf) / 3.0;
        float box_prob = box_objectness;

        if (box_prob >= prob_threshold)
        {
            ArmorObject obj;

            Eigen::Matrix<float, 3, 4> apex_norm;
            Eigen::Matrix<float, 3, 4> apex_dst;

            apex_norm << x_1, x_2, x_3, x_4, y_1, y_2, y_3, y_4, 1, 1, 1, 1;

            apex_dst = transform_matrix * apex_norm;

            for (int i = 0; i < 4; i++)
            {
                obj.apex[i] = cv::Point2f(apex_dst(0, i), apex_dst(1, i));
                obj.pts.push_back(obj.apex[i]);
            }

            vector<cv::Point2f> tmp(obj.apex, obj.apex + 4);
            obj.rect = cv::boundingRect(tmp);

            obj.cls = box_class;
            obj.color = box_color;
            obj.prob = box_prob;

            objects.push_back(obj);
        }

    } // point anchor loop
}

/**
 * @brief Calculate intersection area between two objects.
 * @param a Object a.
 * @param b Object b.
 * @return Area of intersection.
 */
static inline float intersection_area(const ArmorObject &a, const ArmorObject &b)
{
    cv::Rect_<float> inter = a.rect & b.rect;
    return inter.area();
}

static void qsort_descent_inplace(std::vector<ArmorObject> &faceobjects, int left, int right)
{
    int i = left;
    int j = right;
    float p = faceobjects[(left + right) / 2].prob;

    while (i <= j)
    {
        while (faceobjects[i].prob > p)
            i++;

        while (faceobjects[j].prob < p)
            j--;

        if (i <= j)
        {
            // swap
            std::swap(faceobjects[i], faceobjects[j]);

            i++;
            j--;
        }
    }

#pragma omp parallel sections
    {
#pragma omp section
        {
            if (left < j)
                qsort_descent_inplace(faceobjects, left, j);
        }
#pragma omp section
        {
            if (i < right)
                qsort_descent_inplace(faceobjects, i, right);
        }
    }
}

static void qsort_descent_inplace(std::vector<ArmorObject> &objects)
{
    if (objects.empty())
        return;

    qsort_descent_inplace(objects, 0, objects.size() - 1);
}

static void nms_sorted_bboxes(std::vector<ArmorObject> &faceobjects, std::vector<int> &picked, float nms_threshold)
{
    picked.clear();

    const int n = faceobjects.size();

    std::vector<float> areas(n);
    for (int i = 0; i < n; i++)
    {
        areas[i] = faceobjects[i].rect.area();
    }

    for (int i = 0; i < n; i++)
    {
        ArmorObject &a = faceobjects[i];

        int keep = 1;
        for (int j = 0; j < (int)picked.size(); j++)
        {
            ArmorObject &b = faceobjects[picked[j]];

            // intersection over union
            float inter_area = intersection_area(a, b);
            float union_area = areas[i] + areas[picked[j]] - inter_area;
            float iou = inter_area / union_area;
            if (iou > nms_threshold)
            {
                keep = 0;
                // Stored for FFT
                if (iou > FFT_MIN_IOU && abs(a.prob - b.prob) < FFT_CONF_ERROR && a.cls == b.cls && a.color == b.color)
                {
                    for (int i = 0; i < 4; i++)
                    {
                        b.pts.push_back(a.apex[i]);
                    }
                }
                // cout<<b.pts_x.size()<<endl;
            }
        }

        if (keep)
            picked.push_back(i);
    }
}

/**
 * @brief Decode outputs.
 * @param prob Original predition output.
 * @param objects Vector of objects predicted.
 * @param img_w Width of Image.
 * @param img_h Height of Image.
 */
static void decodeOutputs(const float *prob, std::vector<ArmorObject> &objects,
                          Eigen::Matrix<float, 3, 3> &transform_matrix, const int img_w, const int img_h)
{
    std::vector<ArmorObject> proposals;
    std::vector<int> strides = {8, 16, 32};
    std::vector<GridAndStride> grid_strides;

    generate_grids_and_stride(INPUT_W, INPUT_H, strides, grid_strides);
    generateYoloxProposals(grid_strides, prob, transform_matrix, BBOX_CONF_THRESH, proposals);
    qsort_descent_inplace(proposals);

    if (proposals.size() >= TOPK)
        proposals.resize(TOPK);
    std::vector<int> picked;
    nms_sorted_bboxes(proposals, picked, NMS_THRESH);
    int count = picked.size();
    objects.resize(count);

    for (int i = 0; i < count; i++)
    {
        objects[i] = proposals[picked[i]];
    }
}

ArmorDetector::ArmorDetector()
{
}

ArmorDetector::~ArmorDetector()
{
}

// TODO:change to your dir
bool ArmorDetector::initModel(string path)
{
    ie.set_property("CPU", ov::enable_profiling(true));

    model = ie.read_model(path);

    compiled_model = ie.compile_model(model, "CPU");

    infer_request = compiled_model.create_infer_request();

    // moutput = infer_request.get_output_tensor(0);

    return true;

    // // 1.Create Runtime Core
    // // ov::Core ie;
    // model = ie.read_model(path);

    // // 2.Compile the model
    // ov::CompiledModel compiled_model = ie.compile_model(path, "GPU");

    // // 3.Create inference request
    // infer_request = compiled_model.create_infer_request();

    // // 4.Set inputs
    // // Get input tensor by index
    // input_tensor = infer_request.get_input_tensor(0);
    // // IR v10 works with converted precisions (i64 -> i32)
    // // auto data = input_tensor.data<int32_t>();
    // // Fill first data ...

    // return true;
}

ArmorDetector::ArmorDetector(string path)
{
    initModel(path);
};

bool ArmorDetector::detect(Mat &src, std::vector<ArmorObject> &objects)
{
    if (src.empty())
    {
        std::cout << " ERROR: 传入了空的src " << std::endl;
        return false;
    }

    cv::Mat pr_img = scaledResize(src, transfrom_matrix);
#ifdef SHOW_INPUT
    namedWindow("network_input", 0);
    imshow("network_input", pr_img);
    waitKey(1);
#endif // SHOW_INPUT
    cv::Mat pre;
    cv::Mat pre_split[3];
    pr_img.convertTo(pre, CV_32F);
    cv::split(pre, pre_split);
    ov::Tensor imgBlob = infer_request.get_input_tensor(0); // just wrap Mat data by Blob::Ptr

    // locked memory holder should be alive all time while access to its buffer happens
    auto blob_data = imgBlob.data<float_t>();

    auto img_offset = INPUT_W * INPUT_H;
    // Copy img into blob
    for (int c = 0; c < 3; c++)
    {
        memcpy(blob_data, pre_split[c].data, INPUT_W * INPUT_H * sizeof(float));
        blob_data += img_offset;
    }

    //     auto t1 = std::chrono::steady_clock::now();
    infer_request.start_async();
    infer_request.wait();
    //     auto t2 = std::chrono::steady_clock::now();
    //     cout<<(float)(std::chrono::duration<double,std::milli>(t2 - t1).count())<<endl;
    // infer_request.GetPerformanceCounts();
    // -----------------------------------------------------------------------------------------------------
    // --------------------------- Step 8. Process output----------------
    // const Blob::Ptr output_blob = infer_request.GetBlob(output_name);
    // MemoryBlob::CPtr moutput = as<MemoryBlob>(output_blob);
    ov::Tensor output_tensor = infer_request.get_output_tensor();
    const float *net_pred = output_tensor.data<float_t>();
    int img_w = src.cols;
    int img_h = src.rows;

    decodeOutputs(net_pred, objects, transfrom_matrix, img_w, img_h);
    for (auto object = objects.begin(); object != objects.end(); ++object)
    {
        // 对候选框预测角点进行平均,降低误差
        if ((*object).pts.size() >= 8)
        {
            auto N = (*object).pts.size();
            cv::Point2f pts_final[4];

            for (int i = 0; i < N; i++)
            {
                pts_final[i % 4] += (*object).pts[i];
            }

            for (int i = 0; i < 4; i++)
            {
                pts_final[i].x = pts_final[i].x / (N / 4);
                pts_final[i].y = pts_final[i].y / (N / 4);
            }

            (*object).apex[0] = pts_final[0];
            (*object).apex[1] = pts_final[1];
            (*object).apex[2] = pts_final[2];
            (*object).apex[3] = pts_final[3];
        }
        (*object).area = (int)(calcTetragonArea((*object).apex));
    }
    if (objects.size() != 0)
    {
        isFindArmor = 1;
        return true;
    }
    else
    {
        isFindArmor = 0;
        return false;
    }
}

void ArmorDetector::display(Mat &image2show, ArmorObject object)
{
    // 绘制十字瞄准线
    line(image2show, Point2f(image2show.size().width / 2, 0),
         Point2f(image2show.size().width / 2, image2show.size().height), {0, 255, 0}, 1);
    line(image2show, Point2f(0, image2show.size().height / 2),
         Point2f(image2show.size().width, image2show.size().height / 2), {0, 255, 0}, 1);

    // 绘制四点
    // for (int i = 0; i < 4; i++) {
    //     circle(image2show, Point(object.apex[i].x, object.apex[i].y), 3, Scalar(100, 200, 0), 5);
    // }
    // 绘制左上角顶点
    // circle(image2show, Point(object.apex->x, object.apex->y),3,Scalar(255, 255, 0),8 );
    circle(image2show, Point(object.apex[0].x, object.apex[0].y), 3, Scalar(255, 0, 0), 5);
    circle(image2show, Point(object.apex[1].x, object.apex[1].y), 3, Scalar(0, 255, 0), 5);
    circle(image2show, Point(object.apex[2].x, object.apex[2].y), 3, Scalar(0, 0, 255), 5);
    circle(image2show, Point(object.apex[3].x, object.apex[3].y), 3, Scalar(255, 255, 0), 5);
    circle(image2show, Point((object.apex[0].x + object.apex[2].x) / 2, (object.apex[0].y + object.apex[2].y) / 2), 5,
           Scalar(255, 255, 255), 5);

    // 绘制装甲板四点矩形
    for (int i = 0; i < 4; i++)
    {
        line(image2show, object.pts[i], object.pts[(i + 1) % 4], Scalar(100, 200, 0), 3);
    }

    // 绘制目标颜色与类别
    int id = object.cls;
    int box_top_x = object.apex->x;
    int box_top_y = object.apex->y;
    if (object.color == 0)
        cv::putText(image2show, "Blue_" + to_string(id), Point(box_top_x + 2, box_top_y), cv::FONT_HERSHEY_TRIPLEX, 1,
                    Scalar(255, 0, 0));
    else if (object.color == 1)
        cv::putText(image2show, "Red_" + to_string(id), Point(box_top_x + 2, box_top_y), cv::FONT_HERSHEY_TRIPLEX, 1,
                    Scalar(0, 0, 255));
    else if (object.color == 2)
        cv::putText(image2show, "None_" + to_string(id), Point(box_top_x + 2, box_top_y), cv::FONT_HERSHEY_TRIPLEX, 1,
                    Scalar(0, 255, 0));
}

int ArmorDetector::getArmorType()
{
    return armor_object.distinguish;
}

int ArmorDetector::isFindTarget()
{
    return isFindArmor;
}
