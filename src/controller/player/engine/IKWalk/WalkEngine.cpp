#include "WalkEngine.hpp"
#include "CubicSpline.hpp"
#include "SmoothSpline.hpp"
#include "Polynom.hpp"
#include "robot/humanoid.hpp"
#include "math/math.hpp"
#include "configuration.hpp"
#include <cmath>
#include <fstream>
#include "core/adapter.hpp"
#include "sensor/motor.hpp"
#include "core/worldmodel.hpp"
#include "logger.hpp"

namespace motion
{
    using namespace Eigen;
    using namespace robot;
    using namespace robot_math;
    using namespace std;

    WalkEngine::WalkEngine()
    {
        string part=CONF->player()+".walk";
        /**
         * Model leg typical length between
         * each rotation axis
         */
        params_.distHipToKnee = ROBOT->A();
        params_.distKneeToAnkle = ROBOT->B();
        params_.distAnkleToGround = ROBOT->C();
        /**
         * Distance between the two feet in lateral
         * axis while in zero position
         */
        params_.distFeetLateral = ROBOT->D();
        /**
         * Complete (two legs) walk cycle frequency
         * in Hertz
         */
        params_.freq = 1.5;
        /**
         * Global gain multiplying all time
         * dependant movement between 0 and 1.
         * Control walk enabled/disabled smoothing.
         * 0 is walk disabled.
         * 1 is walk fully enabled
         */
        params_.enabledGain = 1.0;
        /**
         * Length of double support phase
         * in phase time
         * (between 0 and 1)
         * 0 is null double support and full single support
         * 1 is full double support and null single support
         */
        params_.supportPhaseRatio = 0.1;
        /**
         * Lateral offset on default foot 
         * position in meters (foot lateral distance)
         * 0 is default
         * > 0 is both feet external offset
         */
        params_.footYOffset = 0.035;
        /**
         * Forward length of each foot step
         * in meters
         * >0 goes forward
         * <0 goes backward
         * (dynamic parameter)
         */
        params_.stepGain = 0.0;
        /**
         * Vertical rise height of each foot
         * in meters (positive)
         */
        params_.riseGain = 0.035;
        /**
         * Angular yaw rotation of each 
         * foot for each step in radian.
         * 0 does not turn
         * >0 turns left
         * <0 turns right
         * (dynamic parameter)
         */
        params_.turnGain = 0.0;
        /**
         * Lateral length of each foot step
         * in meters.
         * >0 goes left
         * <0 goes right
         * (dynamic parameter)
         */
        params_.lateralGain = 0.0;
        /**
         * Vertical foot offset from trunk 
         * in meters (positive)
         * 0 is in init position
         * > 0 set the robot lower to the ground
         */
        params_.trunkZOffset = 0.02;
        /**
         * Lateral trunk oscillation amplitude
         * in meters (positive)
         */
        params_.swingGain = 0.02;
        /**
         * Lateral angular oscillation amplitude
         * of swing trunkRoll in radian
         */
        params_.swingRollGain = 0.0;
        /**
         * Phase shift of lateral trunk oscillation
         * between 0 and 1
         */
        params_.swingPhase = 0.25;
        /**
         * Foot X-Z spline velocities
         * at ground take off and ground landing.
         * Step stands for X and rise stands for Z
         * velocities.
         * Typical values ranges within 0 and 5.
         * >0 for DownVel is having the foot touching the
         * ground with backward velocity.
         * >0 for UpVel is having the foot going back
         * forward with non perpendicular tangent.
         */
        params_.stepUpVel = 4.0;
        params_.stepDownVel = 0.0;
        params_.riseUpVel = 4.0;
        params_.riseDownVel = 0.0;
        /**
         * Time length in phase time
         * where swing lateral oscillation
         * remains on the same side
         * between 0 and 0.5
         */
        params_.swingPause = 0.0;
        /**
         * Swing lateral spline velocity (positive).
         * Control the "smoothness" of swing trajectory.
         * Typical values are between 0 and 5.
         */
        params_.swingVel = 4.0;
        /**
         * Forward trunk-foot offset 
         * with respect to foot in meters
         * >0 moves the trunk forward
         * <0 moves the trunk backward
         */
        params_.trunkXOffset = 0.01;
        /**
         * Lateral trunk-foot offset
         * with respect to foot in meters
         * >0 moves the trunk on the left
         * <0 moves the trunk on the right
         */
        params_.trunkYOffset = 0.0;
        /**
         * Trunk angular rotation
         * around Y in radian
         * >0 bends the trunk forward
         * <0 bends the trunk backward
         */
        params_.trunkPitch = 0.15;
        /**
         * Trunk angular rotation
         * around X in radian
         * >0 bends the trunk on the right
         * <0 bends the trunk on the left
         */
        params_.trunkRoll = 0.0;
        /**
         * Add extra offset on X, Y and Z
         * direction on left and right feet
         * in meters
         * (Can be used for example to implement 
         * dynamic kick)
         */
        params_.extraLeftX = 0.0;
        params_.extraLeftY = 0.0;
        params_.extraLeftZ = 0.0;
        params_.extraRightX = 0.0;
        params_.extraRightY = 0.0;
        params_.extraRightZ = 0.0;
        /**
         * Add extra angular offset on
         * Yaw, Pitch and Roll rotation of 
         * left and right foot in radians
         */
        params_.extraLeftYaw = 0.0;
        params_.extraLeftPitch = 0.0;
        params_.extraLeftRoll = 0.0;
        params_.extraRightYaw = 0.0;
        params_.extraRightPitch = 0.0;
        params_.extraRightRoll = 0.0;

        //The walk is started while walking on place
        params_.enabledGain = 0.0;
        params_.stepGain = 0.0;
        params_.lateralGain = 0.0;
        params_.turnGain = 0.0;

        phase_ = 0.0;
        time_ = 0.0;
        engine_frequency_ = 50.0;
        time_length_ = 1.0/params_.freq;

        x0_ = 0.0;
        y0_ = 0.0;
        d0_ = 0.0;
    }

    void WalkEngine::updata(const pub_ptr &pub, const int &type)
    {
        if (type == sensor::SENSOR_IMU)
        {
            std::shared_ptr<imu> sptr = std::dynamic_pointer_cast<imu>(pub);
            imu_mtx_.lock();
            imu_data_ = sptr->data();
            imu_mtx_.unlock();
            return;
        }

        if (type == sensor::SENSOR_MOTOR)
        {
            std::shared_ptr<motor> sptr = std::dynamic_pointer_cast<motor>(pub);
            dxl_mtx_.lock();
            
            dxl_mtx_.unlock();
            return;
        }
    }

    WalkEngine::~WalkEngine()
    {
        if (td_.joinable())
        {
            td_.join();
        }
        LOG(LOG_INFO) << std::setw(12) << "engine:" << std::setw(18) << "[WalkEngine]" << " ended!" << endll;
    }

    void WalkEngine::start()
    {
        is_alive_ = true;
        td_ = std::move(std::thread(&WalkEngine::run, this));
        LOG(LOG_INFO) << std::setw(12) << "engine:" << std::setw(18) << "[WalkEngine]" << " started!" << endll;
    }

    void WalkEngine::set_params(float x, float y, float d, bool enable)
    {
        para_mutex_.lock();
        params_.stepGain = x;
        params_.lateralGain = y;
        params_.turnGain = d;
        params_.enabledGain = enable?1.0:0.0;
        //bound(xrange[0], xrange[1], params_.stepGain);
        //bound(yrange[0], yrange[1], params_.lateralGain);
        //bound(drange[0], drange[1], params_.turnGain);
        params_.turnGain = deg2rad(params_.turnGain);
        para_mutex_.unlock();
    }

    void WalkEngine::run()
    {   
        Rhoban::IKWalkParameters tempParams;
        std::map<int, float> jdegs;

        while (is_alive_)
        {   
            para_mutex_.lock();
            tempParams = params_;
            para_mutex_.unlock();

            if (MADT->get_mode() == adapter::MODE_READY)
            {
                tempParams.stepGain = 0.0;
                tempParams.lateralGain = 0.0;
                tempParams.turnGain = 0.0;
            }
            
            if(float_equals(tempParams.enabledGain, 1.0) &&(MADT->get_mode() == adapter::MODE_READY || MADT->get_mode() == adapter::MODE_WALK))
            {  
                if( (!is_zero(tempParams.stepGain) && (!is_zero(y0_) || !is_zero(d0_)))
                    || (!is_zero(tempParams.lateralGain) && (!is_zero(x0_) || !is_zero(d0_)))
                    || (!is_zero(tempParams.turnGain) && (!is_zero(x0_) || !is_zero(y0_))) )
                {
                        tempParams.stepGain = 0.0;
                        tempParams.lateralGain = 0.0;
                        tempParams.turnGain = 0.0;
                }

                //Leg motor computed positions
                struct Rhoban::IKWalkOutputs outputs;
                for (double t=0.0;t<=time_length_;t+=1.0/engine_frequency_) 
                {
                    time_ += 1.0/engine_frequency_;
                    bool success = Rhoban::IKWalk::walk(tempParams, 1.0/engine_frequency_, phase_, outputs);
                    if (!success) 
                    {
                        LOG(LOG_ERROR) << time_ << " Inverse Kinematics error. Position not reachable." << endll;
                    } 
                    else 
                    {
                        jdegs[ROBOT->get_joint("jlhip3")->jid_] = rad2deg(outputs.left_hip_yaw);
                        jdegs[ROBOT->get_joint("jlhip2")->jid_] = rad2deg(outputs.left_hip_roll);
                        jdegs[ROBOT->get_joint("jlhip1")->jid_] = rad2deg(outputs.left_hip_pitch);
                        jdegs[ROBOT->get_joint("jlknee")->jid_] = rad2deg(outputs.left_knee);
                        jdegs[ROBOT->get_joint("jlankle2")->jid_] = rad2deg(outputs.left_ankle_pitch);
                        jdegs[ROBOT->get_joint("jlankle1")->jid_] = rad2deg(outputs.left_ankle_roll);

                        jdegs[ROBOT->get_joint("jrhip3")->jid_] = rad2deg(outputs.right_hip_yaw);
                        jdegs[ROBOT->get_joint("jrhip2")->jid_] = rad2deg(outputs.right_hip_roll);
                        jdegs[ROBOT->get_joint("jrhip1")->jid_] = rad2deg(outputs.right_hip_pitch);
                        jdegs[ROBOT->get_joint("jrknee")->jid_] = rad2deg(outputs.right_knee);
                        jdegs[ROBOT->get_joint("jrankle2")->jid_] = rad2deg(outputs.right_ankle_pitch);
                        jdegs[ROBOT->get_joint("jrankle1")->jid_] = rad2deg(outputs.right_ankle_roll);

                        jdegs[ROBOT->get_joint("jlshoulder1")->jid_] = 40;
                        jdegs[ROBOT->get_joint("jlelbow")->jid_] = -90;
                        jdegs[ROBOT->get_joint("jrshoulder1")->jid_] = 40;
                        jdegs[ROBOT->get_joint("jrelbow")->jid_] = 90;

                        while (!MADT->body_empty())
                        {
                            usleep(500);
                        }
                        if (!MADT->add_body_degs(jdegs))
                        {
                            break;
                        } 
                    }
                }
                x0_ = tempParams.stepGain;
                y0_ = tempParams.lateralGain;
                d0_ = tempParams.turnGain;
                WM->navigation(Vector3d(tempParams.stepGain, tempParams.lateralGain, tempParams.turnGain));

                if (MADT->get_mode() == adapter::MODE_READY)
                {
                    para_mutex_.lock();
                    params_.enabledGain = 0.0;
                    para_mutex_.unlock();
                    if(MADT->get_last_mode() == adapter::MODE_ACT)
                        MADT->set_mode(adapter::MODE_WALK);
                    else
                        MADT->set_mode(adapter::MODE_ACT);
                }                  
            }
            usleep(500);
        }
    }
}