#ifndef YOLOXARMOR_GENERAL_H
#define YOLOXARMOR_GENERAL_H

#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <openvino/runtime/properties.hpp>
#include <string>
#include <unistd.h>
#include <vector>

#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Dense>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

enum TargetType
{
    SMALL,
    BIG,
    BUFF
};

/**
 * @brief 存储任务所需数据的结构体
 *
 */
struct TaskData
{
    int mode;
    double bullet_speed;
    Mat img;
    Eigen::Quaterniond quat;
    int timestamp; // 单位：ms
};

struct GridAndStride
{
    int grid0;
    int grid1;
    int stride;
};

template <typename T> bool initMatrix(Eigen::MatrixXd &matrix, std::vector<T> &vector)
{
    int cnt = 0;
    for (int row = 0; row < matrix.rows(); row++)
    {
        for (int col = 0; col < matrix.cols(); col++)
        {
            matrix(row, col) = vector[cnt];
            cnt++;
        }
    }
    return true;
}

float calcTriangleArea(cv::Point2f pts[3]);
float calcTetragonArea(cv::Point2f pts[4]);
double rangedAngleRad(double &angle);

Eigen::Vector3d rotationMatrixToEulerAngles(Eigen::Matrix3d &R);

Eigen::Vector3d calcDeltaEuler(Eigen::Vector3d euler1, Eigen::Vector3d euler2);
Eigen::AngleAxisd eulerToAngleAxisd(Eigen::Vector3d euler);
Eigen::Matrix3d eulerToRotationMatrix(Eigen::Vector3d &theta);

#endif // YOLOXARMOR_GENERAL_H
