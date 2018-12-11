#pragma once

#include <thread>
#include <memory>
#include <atomic>
#include "singleton.hpp"
#include "gpio/gpio.hpp"

enum led_state
{
    LED_CUSTOM = 0,
    LED_NORMAL = 1,
    LED_WARN = 2,
    LED_ERROR = 3
};

class LedEngine: public singleton<LedEngine>
{
public:
    LedEngine();
    ~LedEngine();
    void start();
    void stop();
    bool set_led(int idx, bool status);

    inline void set_state(led_state s)
    {
        led_state_ = s;
    }
    
private:
    void run();
    std::thread td_;
    bool is_alive_;
    bool can_use_;

    unsigned int count_;
    unsigned int custom_;

    std::atomic_int led_state_;
    std::atomic_bool led1_status_;
    std::atomic_bool led2_status_;

    std::shared_ptr<gpio> led1_;
    std::shared_ptr<gpio> led2_;
};

#define LE LedEngine::instance()