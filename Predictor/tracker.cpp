// Copyright 2022 Chen Jun

#include "tracker.hpp"

#include <Eigen/src/Geometry/Quaternion.h>
#include <cstddef>

// STD
#include <cfloat>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace rm_auto_aim
{
Tracker::Tracker(double max_match_distance, double max_match_yaw_diff)
    : tracker_state(LOST), tracked_id(0), measurement(Eigen::VectorXd::Zero(4)), target_state(Eigen::VectorXd::Zero(9)),
      max_match_distance_(max_match_distance), max_match_yaw_diff_(max_match_yaw_diff)
{
}

void Tracker::init(const std::shared_ptr<Armors> &armors_msg)
{
    if (armors_msg->armors.empty())
    {
        return;
    }

    // Simply choose the armor that is closest to image center
    double min_distance = DBL_MAX;
    tracked_armor = armors_msg->armors[0];
    for (const auto &armor : armors_msg->armors)
    {
        if (armor.distance_to_image_center < min_distance)
        {
            min_distance = armor.distance_to_image_center;
            tracked_armor = armor;
        }
    }

    initEKF(tracked_armor);

    tracked_id = tracked_armor.number;
    tracker_state = DETECTING;

    updateArmorsNum(tracked_armor);
}

void Tracker::update(const std::shared_ptr<Armors> &armors_msg)
{
    // KF predict
    Eigen::VectorXd ekf_prediction = ekf.predict();

    bool matched = false;
    // Use KF prediction as default target state if no matched armor is found
    target_state = ekf_prediction;

    if (!armors_msg->armors.empty())
    {
        // Find the closest armor with the same id
        Armor same_id_armor;
        int same_id_armors_count = 0;
        auto predicted_position = getArmorPositionFromState(ekf_prediction);
        double min_position_diff = DBL_MAX;
        double yaw_diff = DBL_MAX;
        for (const auto &armor : armors_msg->armors)
        {
            // Only consider armors with the same id
            if (armor.number == tracked_id)
            {
                same_id_armor = armor;
                same_id_armors_count++;
                // Calculate the difference between the predicted position and the current armor position
                auto p = armor.pose.position;
                Eigen::Vector3d position_vec(p.x, p.y, p.z);
                double position_diff = (predicted_position - position_vec).norm();
                if (position_diff < min_position_diff)
                {
                    // Find the closest armor
                    min_position_diff = position_diff;
                    yaw_diff = abs(orientationToYaw(armor.pose.orientation) - ekf_prediction(6));
                    tracked_armor = armor;
                }
            }
        }

        // Store tracker info
        info_position_diff = min_position_diff;
        info_yaw_diff = yaw_diff;

        // Check if the distance and yaw difference of closest armor are within the threshold
        if (min_position_diff < max_match_distance_ && yaw_diff < max_match_yaw_diff_)
        {
            // Matched armor found
            matched = true;
            auto p = tracked_armor.pose.position;
            // Update EKF
            double measured_yaw = orientationToYaw(tracked_armor.pose.orientation);
            measurement = Eigen::Vector4d(p.x, p.y, p.z, measured_yaw);
            target_state = ekf.update(measurement);
            std::cout << "armor_tracker:"
                      << "EKF update\n";
        }
        else if (same_id_armors_count == 1 && yaw_diff > max_match_yaw_diff_)
        {
            // Matched armor not found, but there is only one armor with the same id
            // and yaw has jumped, take this case as the target is spinning and armor jumped
            handleArmorJump(same_id_armor);
        }
        else
        {
            // No matched armor found
            std::cout << "armor_tracker:"
                      << "No matched armor found!\n";
        }
    }

    // Prevent radius from spreading
    if (target_state(8) < 0.12)
    {
        target_state(8) = 0.12;
        ekf.setState(target_state);
    }
    else if (target_state(8) > 0.4)
    {
        target_state(8) = 0.4;
        ekf.setState(target_state);
    }

    // Tracking state machine
    if (tracker_state == DETECTING)
    {
        if (matched)
        {
            detect_count_++;
            if (detect_count_ > tracking_thres)
            {
                detect_count_ = 0;
                tracker_state = TRACKING;
            }
        }
        else
        {
            detect_count_ = 0;
            tracker_state = LOST;
        }
    }
    else if (tracker_state == TRACKING)
    {
        if (!matched)
        {
            tracker_state = TEMP_LOST;
            lost_count_++;
        }
    }
    else if (tracker_state == TEMP_LOST)
    {
        if (!matched)
        {
            lost_count_++;
            if (lost_count_ > lost_thres)
            {
                lost_count_ = 0;
                tracker_state = LOST;
            }
        }
        else
        {
            tracker_state = TRACKING;
            lost_count_ = 0;
        }
    }
}

void Tracker::initEKF(const Armor &a)
{
    double xa = a.pose.position.x;
    double ya = a.pose.position.y;
    double za = a.pose.position.z;
    last_yaw_ = 0;
    double yaw = orientationToYaw(a.pose.orientation);

    // Set initial position at 0.2m behind the target
    target_state = Eigen::VectorXd::Zero(9);
    double r = 0.26;
    double xc = xa + r * cos(yaw);
    double yc = ya + r * sin(yaw);
    dz = 0, another_r = r;
    target_state << xc, 0, yc, 0, za, 0, yaw, 0, r;

    ekf.setState(target_state);
}

void Tracker::updateArmorsNum(const Armor &armor)
{
    if (armor.type == "large" && (tracked_id == "3" || tracked_id == "4" || tracked_id == "5"))
    {
        tracked_armors_num = ArmorsNum::BALANCE_2;
    }
    else if (tracked_id == "outpost")
    {
        tracked_armors_num = ArmorsNum::OUTPOST_3;
    }
    else
    {
        tracked_armors_num = ArmorsNum::NORMAL_4;
    }
}

void Tracker::handleArmorJump(const Armor &current_armor)
{
    double yaw = orientationToYaw(current_armor.pose.orientation);
    target_state(6) = yaw;
    updateArmorsNum(current_armor);
    // Only 4 armors has 2 radius and height
    if (tracked_armors_num == ArmorsNum::NORMAL_4)
    {
        dz = target_state(4) - current_armor.pose.position.z;
        target_state(4) = current_armor.pose.position.z;
        std::swap(target_state(8), another_r);
    }

    // If position difference is larger than max_match_distance_,
    // take this case as the ekf diverged, reset the state
    auto p = current_armor.pose.position;
    Eigen::Vector3d current_p(p.x, p.y, p.z);
    Eigen::Vector3d infer_p = getArmorPositionFromState(target_state);
    if ((current_p - infer_p).norm() > max_match_distance_)
    {
        double r = target_state(8);
        target_state(0) = p.x + r * cos(yaw); // xc
        target_state(1) = 0;                  // vxc
        target_state(2) = p.y + r * sin(yaw); // yc
        target_state(3) = 0;                  // vyc
        target_state(4) = p.z;                // za
        target_state(5) = 0;                  // vza
    }

    ekf.setState(target_state);
}

double shortest_angular_distance(double angle1, double angle2)
{
    const double two_pi = 2 * M_PI;
    const double pi = M_PI;

    // Normalize angles to -pi~pi range
    angle1 = fmod(angle1, two_pi);
    angle2 = fmod(angle2, two_pi);

    // Make sure angle2 is greater than angle1
    if (angle2 < angle1)
    {
        angle2 += two_pi;
    }

    // Calculate the difference between angle2 and angle1
    double diff = angle2 - angle1;

    // Check for angle wrapping and adjust the difference accordingly
    if (diff > pi)
    {
        diff = diff - two_pi;
    }
    else if (diff < -pi)
    {
        diff = diff + two_pi;
    }

    return diff;
}

double Tracker::orientationToYaw(const Eigen::Quaterniond &q)
{
    // Get armor yaw
    // Eigen::Quaterniond tf_q(q.w, q.x, q.y, q.z);
    // tf2::fromMsg(q, tf_q);
    // double roll, pitch, yaw;
    // tf2::Matrix3x3(tf_q).getRPY(roll, pitch, yaw);
    double x = q.x();
    double y = q.y();
    double z = q.z();
    double w = q.w();
    double r = x * x + y * y - z * z - w * w;
    double p = 2 * (x * y + w * z);
    double yaw = atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y));

    // Make yaw change continuous (-pi~pi to -inf~inf)
    yaw = last_yaw_ + shortest_angular_distance(last_yaw_, yaw);
    last_yaw_ = yaw;
    return yaw;
}

Eigen::Vector3d Tracker::getArmorPositionFromState(const Eigen::VectorXd &x)
{
    // Calculate predicted position of the current armor
    double xc = x(0), yc = x(2), za = x(4);
    double yaw = x(6), r = x(8);
    double xa = xc - r * cos(yaw);
    double ya = yc - r * sin(yaw);
    return Eigen::Vector3d(xa, ya, za);
}

} // namespace rm_auto_aim
