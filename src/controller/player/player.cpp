#include "player.hpp"
#include "configuration.hpp"
#include "task/action_task.hpp"
#include "task/look_task.hpp"
#include "task/walk_task.hpp"
#include "task/gcret_task.hpp"
#include "task/say_task.hpp"
#include "server/server.hpp"
#include "core/adapter.hpp"
#include "engine/walk/WalkEngine.hpp"
#include "engine/scan/ScanEngine.hpp"
#include "engine/action/ActionEngine.hpp"
#include "engine/led/LedEngine.hpp"

using namespace std;
using namespace motion;

const map<string, player::PlayerRole> player::RoleName{{"keeper", player::KEEPER}, {"attacker", player::ATTACKER}, {"guard", player::GUARD}};

player::player(): timer(CONF->get_config_value<int>("think_period"))
{
    is_alive_ = false;
    period_count_ = 0;
    btn_count_ = 0;
    role_ = RoleName.find(CONF->get_config_value<string>(CONF->player()+".strategy.role"))->second;
    state_ = STATE_NOTMAL;
    w_ = CONF->get_config_value<int>("image.width");
    h_ = CONF->get_config_value<int>("image.height");
}

void player::run()
{
    if (is_alive_)
    {
        period_count_++;

        if (OPTS->use_robot())
        {
            if(WM->button_status(1)&&WM->button_status(2))
            {
                btn_count_++;
                if(btn_count_%40==0)
                    raise(SIGINT);
            }
            else
            {
                btn_count_=0;
            }
        }
        if(OPTS->use_remote())
        {
            play_with_remote();
        }
        else
        {
            list<task_ptr> tasks, tlist;
            if(period_count_%10 == 0)
            {
                if(OPTS->use_gc())
                    tasks.push_back(make_shared<gcret_task>());
                if(OPTS->use_comm())
                    tasks.push_back(make_shared<say_task>());
            }
            tlist = think();
            tasks.insert(tasks.end(), tlist.begin(), tlist.end());    
            
            for(auto &tsk:tasks)
            {
                if(tsk.get())
                    tsk->perform();
            }
        }
    }
}

list<task_ptr> player::think()
{
    list<task_ptr> tasks, tlists;
    if(OPTS->image_record())
    {
        //tasks.push_back(make_shared<look_task>(true));
        //tasks.push_back(make_shared<walk_task>(0.0, 0.0, 0.0, true));
    }
    else if(OPTS->use_gc())
    {
        tlists = play_with_gc();
    }
    else
    {
        tlists = play_without_gc();
        //tasks.push_back(make_shared<look_task>(true));
        //tasks.push_back(make_shared<walk_task>(0.0, 0.0, -10.0, true));
    }
    //tasks.push_back(make_shared<look_task>(true));
    //tasks.push_back(make_shared<walk_task>(0.0, 0.0, 0.0, true));
    //tasks.push_back(make_shared<action_task>("reset"));
    tasks.insert(tasks.end(), tlists.begin(), tlists.end());
    return tasks;
}

bool player::init()
{
    if(OPTS->use_debug())
        SERVER->start();

    if (!regist())
    {
        return false;
    }

    is_alive_ = true;

    if (OPTS->use_robot())
    {
        while (!dynamic_pointer_cast<motor>(sensors_["motor"])->is_connected() && is_alive_)
        {
            LOG(LOG_WARN) << "waiting for motor connection, please turn on the power." << endll;
            sleep(1);
        }

        if (!is_alive_)
        {
            return true;
        }
    }

    MADT->start();
    WE->start();
    SE->start();
    AE->start();
    if(OPTS->use_robot())
    {
        LE->start();
    }
    action_task p("ready");
    p.perform();
    start_timer();
    return true;
}

void player::stop()
{
    is_alive_ = false;
    WE->stop();
    SE->stop();
    AE->stop();
    if(OPTS->use_robot())
    {
        LE->stop();
    }
    MADT->stop();

    if (is_alive_)
    {
        delete_timer();
    }

    sleep(1);
    unregist();
    sleep(1);
    if(OPTS->use_debug())
        SERVER->stop();
}

bool player::regist()
{
    sensors_.clear();

    if (OPTS->use_camera())
    {
        sensors_["camera"] = std::make_shared<camera>();
        sensors_["camera"]->attach(VISION);
        sensors_["camera"]->start();

        if (!VISION->start())
        {
            return false;
        }
    }

    sensors_["motor"] = std::make_shared<motor>();
    sensors_["motor"]->attach(WE);

    if (!sensors_["motor"]->start())
    {
        return false;
    }

    if (OPTS->use_robot())
    {
        sensors_["imu"] = std::make_shared<imu>();
        sensors_["imu"]->attach(WM);
        sensors_["imu"]->attach(VISION);
        sensors_["imu"]->attach(WE);
        sensors_["imu"]->start();
        
        sensors_["button"] = std::make_shared<button>();
        sensors_["button"]->attach(WM);
        sensors_["button"]->start();
    }

    if (OPTS->use_gc())
    {
        sensors_["gamectrl"] = std::make_shared<gamectrl>();
        sensors_["gamectrl"]->attach(WM);
        sensors_["gamectrl"]->start();
    }

    if (OPTS->use_comm())
    {
        sensors_["hear"] = std::make_shared<hear>();
        sensors_["hear"]->attach(WM);
        sensors_["hear"]->start();
    }

    return true;
}

void player::unregist()
{
    if (sensors_.find("button") != sensors_.end())
    {
        sensors_["button"]->detach(WM);
        sensors_["button"]->stop();
    }
    if (sensors_.find("imu") != sensors_.end())
    {
        sensors_["imu"]->detach(WM);
        sensors_["imu"]->detach(VISION);
        sensors_["imu"]->detach(WE);
        sensors_["imu"]->stop();
    }
    if (sensors_.find("motor") != sensors_.end())
    {
        sensors_["motor"]->detach(WE);
        sensors_["motor"]->stop();
    }
    if (sensors_.find("camera") != sensors_.end())
    {
        sensors_["camera"]->detach(VISION);
        sensors_["camera"]->stop();
        VISION->stop();
    }
    if (sensors_.find("gamectrl") != sensors_.end())
    {
        sensors_["gamectrl"]->detach(WM);
        sensors_["gamectrl"]->stop();
    }
    if (sensors_.find("hear") != sensors_.end())
    {
        sensors_["hear"]->detach(WM);
        sensors_["hear"]->stop();
    }
}

sensor_ptr player::get_sensor(const std::string &name)
{
    auto iter = sensors_.find(name);

    if (iter != sensors_.end())
    {
        return iter->second;
    }

    return nullptr;
}