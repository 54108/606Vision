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
#include <iostream>
#include <opencv2/opencv.hpp>

// Main code
int main(int, char **)
{
    // 初始化相机
    mindvision::MVCamera *mv_capture_ = new mindvision::MVCamera(
        mindvision::CameraParam(0, mindvision::RESOLUTION_1280_X_1024, mindvision::EXPOSURE_5000));
    cv::Mat src_img_;

    while (mv_capture_->isCameraOnline())
    {
        src_img_ = mv_capture_->image();
        // do something

        imshow("output", src_img_);
        cv::waitKey(1);

        mv_capture_->releaseBuff();
    }

    return 0;
}
