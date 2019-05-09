#include "worldmodel.hpp"
#include "localization/SoccerMap.h"

using namespace Eigen;
using namespace robot_math;
using namespace robot;
using namespace std;

world_model::world_model()
{
    fall_direction_ = FALL_NONE;
    support_foot_ = robot::DOUBLE_SUPPORT;
    player_infos_[CONF->id()].id = CONF->id();
    opp_post_left = Vector2d(SOCCERMAP->width()/200.0, SOCCERMAP->goalWidth()/200.0-0.7);
    opp_post_right = Vector2d(SOCCERMAP->width()/200.0, -SOCCERMAP->goalWidth()/200.0+0.7);
}

void world_model::updata(const pub_ptr &pub, const int &type)
{
    if (type == sensor::SENSOR_IMU)
    {
        std::shared_ptr<imu> sptr = std::dynamic_pointer_cast<imu>(pub);
        imu_mtx_.lock();
        imu_data_ = sptr->data();
        fall_direction_ = sptr->fall_direction();
        imu_mtx_.unlock();
        self_mtx_.lock();
        self_block_.dir = sptr->data().yaw;
        self_mtx_.unlock();  
        info_mtx_.lock();
        player_infos_[CONF->id()].dir = sptr->data().yaw;
        info_mtx_.unlock();        
        return;
    }

    if(type == sensor::SENSOR_BUTTON)
    {
        std::shared_ptr<button> sptr = std::dynamic_pointer_cast<button>(pub);
        bt1_status_ = sptr->button_1();
        bt2_status_ = sptr->button_2();
        return;
    }

    if(type == sensor::SENSOR_GC)
    {
        std::shared_ptr<gamectrl> sptr = std::dynamic_pointer_cast<gamectrl>(pub);
        gc_mtx_.lock();
        gc_data_ = sptr->data();
        gc_mtx_.unlock();
        return;
    }

    if(type == sensor::SENSOR_HEAR)
    {
        std::shared_ptr<hear> sptr = std::dynamic_pointer_cast<hear>(pub);
        info_mtx_.lock();
        player_info info = sptr->info();
        player_infos_[info.id] = info;
        info_mtx_.unlock();
        return;
    }
}

void world_model::set_my_pos(const Eigen::Vector2d &my)
{
    info_mtx_.lock();
    player_infos_[CONF->id()].x = my.x();
    player_infos_[CONF->id()].y = my.y();
    info_mtx_.unlock();
    self_mtx_.lock();
    self_block_.global = my;
    self_mtx_.unlock();
}

void world_model::set_ball_pos(const Eigen::Vector2d &global, const Eigen::Vector2d &my, const Eigen::Vector2i &pix,
    float alpha, float beta,  bool can)
{
    info_mtx_.lock();
    player_infos_[CONF->id()].ball_x = global.x();
    player_infos_[CONF->id()].ball_y = global.y();
    player_infos_[CONF->id()].can_see = can;
    info_mtx_.unlock();
    
    ball_mtx_.lock();
    ball_block_.global = global;
    ball_block_.self = my;
    ball_block_.pixel = pix;
    ball_block_.alpha = alpha;
    ball_block_.beta = beta;
    ball_block_.can_see = can;
    ball_mtx_.unlock();
}

void world_model::reset_hear_info()
{
    info_mtx_.lock();
    for(auto &item:player_infos_)
    {
        if(item.first != CONF->id())
            item.second.can_see = false;
    }
    info_mtx_.unlock();
}
