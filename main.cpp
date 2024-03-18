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
#include "Utils/msg.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>

msg::Armor armor_msg;
msg::Armors armors_msg;

void tracker(){

};

// Main code
int main(int, char **)
{
    // 初始化相机
    mindvision::MVCamera *mv_capture_ = new mindvision::MVCamera(
        mindvision::CameraParam(0, mindvision::RESOLUTION_1280_X_1024, mindvision::EXPOSURE_5000));
    cv::Mat src_img_;

    std::vector<cv::Point2f> image_points;
    std::vector<armor_detector::ArmorObject> objects;

    // 初始化网络模型
    const string network_path = "Detector/model/opt-0517-001.xml";
    armor_detector::ArmorDetector armor_detector(network_path);


    while (mv_capture_->isCameraOnline())
    {
        src_img_ = mv_capture_->image();
        // do something

        if (armor_detector.detect(src_img_, objects))
        {
            for (auto armor_object : objects)
            {
                armor_detector.display(src_img_, armor_object); // 识别结果可视化
            }
        }

        imshow("output", src_img_);
        cv::waitKey(1);

        mv_capture_->releaseBuff();
    }

    return 0;
}
