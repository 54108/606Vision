// Copyright 2022 Chen Jun

#ifndef ARMOR_PROCESSOR__PROCESSOR_NODE_HPP_
#define ARMOR_PROCESSOR__PROCESSOR_NODE_HPP_

#include "SolveTrajectory.hpp"
#include "Utils/msg.hpp"
#include "core/persistence.hpp"
#include "eigen3/Eigen/Core"
#include "tracker.hpp"
#include <memory>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace rm_auto_aim
{
class ArmorTrackerNode
{
  public:
    explicit ArmorTrackerNode(std::string path, msg::Receive receive_msg_);

    // int declare_Parameter(std::string s, int a);
    // double declare_Parameter(std::string s, double a);
    template <typename T> auto declare_Parameter(std::string s, T t)
    {
        return t;
    };

  private:
    void velocityCallback(const std::shared_ptr<msg::Velocity> velocity_msg);

    void armorsCallback(const std::shared_ptr<msg::Armors> armors_msg, msg::TrackerInfo info_msg,
                        msg::Target target_msg, msg::Send send_msg);

    // void publishMarkers(const msg::Target &target_msg);

    // Maximum allowable armor distance in the XOY plane
    double max_armor_distance_;

    std::chrono::time_point<std::chrono::system_clock> time_;
    std::chrono::time_point<std::chrono::system_clock> last_time_;

    double dt_;

    std::string path;
    Eigen::Quaterniond target_frame_;
    msg::Receive receive_msg_;

    // Armor tracker
    double s2qxyz_, s2qyaw_, s2qr_;
    double r_xyz_factor, r_yaw;
    double lost_time_thres_;
    std::unique_ptr<Tracker> tracker_;
    std::unique_ptr<SolveTrajectory> gaf_solver;
};

} // namespace rm_auto_aim

#endif // ARMOR_PROCESSOR__PROCESSOR_NODE_HPP_
