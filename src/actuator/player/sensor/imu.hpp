#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <thread>
#include "sensor.hpp"

class imu: public sensor
{
public:
    struct imu_data
    {
        float pitch, roll, yaw;
        float ax, ay, az;
        float wx, wy, wz;
    };

    imu();
    ~imu();

    bool start();
    void stop();

    void data_handler(const char *data, const int &size, const int &type = 0);

    imu_data data() const
    {
        return data_;
    }

private:
    bool open();
    void read_head0();
    void read_head1();
    void read_data();

    enum {imu_data_size = sizeof(imu_data)};
    enum {imu_len = 11};
    unsigned char buff_[imu_len];
    imu_data data_;
    std::thread td_;
    boost::asio::serial_port serial_;

};
