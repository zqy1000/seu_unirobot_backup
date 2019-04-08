#pragma once

#include <map>
#include <thread>
#include <mutex>
#include <eigen3/Eigen/Dense>
#include "observer.hpp"
#include "sensor/imu.hpp"
#include "robot/robot_define.hpp"
#include "singleton.hpp"
#include "IKWalk.hpp"

namespace motion
{
    enum Walk_State
    {
        WALK_STOP,
        WALK_TO_ACT,
        WALK_NORMAL,
        ACT_TO_WALK
    };
    class WalkEngine: public subscriber, public singleton<WalkEngine>
    {
    public:
        WalkEngine();
        ~WalkEngine();
        void start();
        void stop()
        {
            is_alive_ = false;
        }
        void set_params(float x, float y, float d, bool enable);
        void updata(const pub_ptr &pub, const int &type);
        
    private:
        double engine_frequency_;

        double d0_, x0_, y0_, g0_;
        double phase_, time_;
        double time_length_;

        void run();
        Rhoban::IKWalkParameters params_;
        std::thread td_;
        bool is_alive_;
        Eigen::Vector2d xrange, yrange, drange;
        imu::imu_data imu_data_;
        mutable std::mutex para_mutex_, imu_mtx_, dxl_mtx_;
    };

#define WE WalkEngine::instance()
}

