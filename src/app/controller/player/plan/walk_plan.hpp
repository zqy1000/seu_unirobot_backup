#pragma once

#include "plan.hpp"
#include "motion/walk/WalkEngine.hpp"
#include "joints_plan.hpp"

class walk_plan: public plan
{
public:
    walk_plan(const float &x, const float &y, const float &dir, bool enable)
        : plan("walk_plan", "body")
    {
        params_[0] = x;
        params_[1] = y;
        params_[2] = dir;
        enable_ = enable;
    }

    bool perform()
    {
        motion::WE->set_params(params_, enable_);
        MADT->mode() = adapter::MODE_WALK;
        return true;
    }
private:
    Eigen::Vector3d params_;
    bool enable_;
};

