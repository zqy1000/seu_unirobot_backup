#pragma once

#include <mutex>
#include "pattern.hpp"
#include "sensor/imu.hpp"
#include "sensor/motor.hpp"
#include "sensor/gamectrl.hpp"
#include "sensor/server.hpp"
#include "sensor/hear.hpp"
#include "configuration.hpp"
#include "singleton.hpp"

class WorldModel: public subscriber, public singleton<WorldModel>
{
public:
    WorldModel()
    {
        support_foot_ = robot::DOUBLE_SUPPORT;
        rmt_data_.type = NON_DATA;
        rmt_data_.size = 0;
        low_power_ = false;
        lost_ = false;
    }

    void updata(const pro_ptr &pub, const int &type)
    {
        if (type == sensor::SENSOR_GC)
        {
            gc_mtx_.lock();
            std::shared_ptr<gamectrl> sptr = std::dynamic_pointer_cast<gamectrl>(pub);
            gc_data_ = sptr->data();
            gc_mtx_.unlock();
            std::cout << (int)gc_data_.state << std::endl;
            return;
        }

        if (type == sensor::SENSOR_HEAR)
        {
            hear_mtx_.lock();
            std::shared_ptr<hear> sptr = std::dynamic_pointer_cast<hear>(pub);
            players_[sptr->info().id] = sptr->info();
            hear_mtx_.unlock();
            return;
        }

        if (type == sensor::SENSOR_IMU)
        {
            imu_mtx_.lock();
            std::shared_ptr<imu> sptr = std::dynamic_pointer_cast<imu>(pub);
            imu_data_ = sptr->data();
            sw_data_ = sptr->switch_data();
            lost_ = sptr->lost();
            imu_mtx_.unlock();
            return;
        }

        if (type == sensor::SENSOR_MOTOR)
        {
            dxl_mtx_.lock();
            std::shared_ptr<motor> sptr = std::dynamic_pointer_cast<motor>(pub);
            low_power_ = sptr->low_power();
            dxl_mtx_.unlock();
            return;
        }

        if (type == sensor::SENSOR_SERVER)
        {
            rmt_mtx_.lock();
            std::shared_ptr<tcp_server> sptr = std::dynamic_pointer_cast<tcp_server>(pub);
            rmt_data_ = sptr->r_data();
            rmt_mtx_.unlock();
            return;
        }

    }

    inline robot::support_foot get_support_foot() const
    {
        std::lock_guard<std::mutex> lk(sf_mtx_);
        return support_foot_;
    }

    inline void set_support_foot(const robot::support_foot &sf)
    {
        std::lock_guard<std::mutex> lk(sf_mtx_);
        support_foot_ = sf;
    }

    inline RoboCupGameControlData gc_data() const
    {
        std::lock_guard<std::mutex> lk(gc_mtx_);
        return gc_data_;
    }

    inline imu::imu_data imu_data() const
    {
        std::lock_guard<std::mutex> lk(imu_mtx_);
        return imu_data_;
    }

    inline sw_data switch_data() const
    {
        std::lock_guard<std::mutex> lk(imu_mtx_);
        return sw_data_;
    }

    inline std::map<int, player_info> players() const
    {
        std::lock_guard<std::mutex> lk(hear_mtx_);
        return players_;
    }

    inline remote_data rmt_data() const
    {
        std::lock_guard<std::mutex> lk(rmt_mtx_);
        return rmt_data_;
    }

    inline void reset_rmt_data()
    {
        std::lock_guard<std::mutex> lk(rmt_mtx_);
        rmt_data_.type = NON_DATA;
        rmt_data_.size = 0;
    }

    inline bool low_power() const
    {
        std::lock_guard<std::mutex> lk(dxl_mtx_);
        return low_power_;
    }

    inline bool lost() const
    {
        std::lock_guard<std::mutex> lk(imu_mtx_);
        return lost_;
    }

private:
    bool low_power_, lost_;
    std::map<int, player_info> players_;
    RoboCupGameControlData gc_data_;
    imu::imu_data imu_data_;
    sw_data sw_data_;
    remote_data rmt_data_;
    robot::support_foot support_foot_;
    mutable std::mutex gc_mtx_, imu_mtx_, rmt_mtx_, dxl_mtx_, hear_mtx_, sf_mtx_;
};

#define WM WorldModel::instance()

