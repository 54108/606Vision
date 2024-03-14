/**
 * @file main.cpp
 * @author 54108 (3318195572@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-03-14
 *
 * @copyright Copyright (c) 2024
 *
 */
#define DEBUG
#ifdef DEBUG
#define DBG std::cout << __FILE__ << ":" << __LINE__ << ":" << __TIME__ << std::endl;
#endif

#include "Camera/MVCamera.hpp"
#include "Detector/ArmorDetector/ArmorDetector.hpp"
#include "Detector/OpenVINO2022/types.hpp"
#include "PoseSolver/PoseSolver.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <semaphore>
#include <thread>

std::binary_semaphore sem_tracker(0);
std::binary_semaphore sem_serial(0);

msg::Armor armor_msg;
msg::Armors armors_msg;

void tracker(){

};

// Main code
int main(int, char **)
{
    // 初始化姿态解算
    PoseSolver poseSolver = PoseSolver("Configs/pose_solver/camera_params.xml", 1);
    // 初始化相机
    mindvision::MVCamera *mv_capture_ = new mindvision::MVCamera(
        mindvision::CameraParam(0, mindvision::RESOLUTION_1280_X_1024, mindvision::EXPOSURE_5000));
    cv::Mat src_img_;

    std::vector<cv::Point2f> image_points;
    armor_detector::ArmorDetector armor_detector;
    std::vector<armor_detector::ArmorObject> objects;

    // 初始化网络模型
    const string network_path = "Detector/model/opt-0517-001.xml";

    armor_detector.initModel(network_path);

    while (mv_capture_->isCameraOnline())
    {
        src_img_ = mv_capture_->image();
        // do something

        if (armor_detector.detect(src_img_, objects))
        {
            for (auto armor_object : objects)
            {
                poseSolver.solvePose(armor_object, armor_msg);
                // putText(src_img_, to_string(global_fps_.getpfs()), Point(0, 24), FONT_HERSHEY_COMPLEX, 1.0,
                //         Scalar(12, 23, 200), 3, 8);
                putText(src_img_, to_string(poseSolver.getYawAngle()), Point(0, 48), FONT_HERSHEY_COMPLEX, 1.0,
                        Scalar(12, 23, 200), 3, 8);
                armor_detector.display(src_img_, armor_object); // 识别结果可视化
            }
        }

        imshow("output", src_img_);
        cv::waitKey(1);

        mv_capture_->releaseBuff();
    }

    return 0;
}
