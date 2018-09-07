#include "image_monitor.hpp"
#include "configuration.hpp"
#include "ui/walk_remote.hpp"
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;
using namespace robot;

image_monitor::image_monitor()
    :client_(CONF->get_config_value<string>(CONF->player()+".address"), CONF->get_config_value<int>("net.tcp.port"),
            bind(&image_monitor::data_handler, this, placeholders::_1))
{
    height_ = CONF->get_config_value<int>("hardware.camera.height");
    width_ = CONF->get_config_value<int>("hardware.camera.width");
    first_connect = true;
    
    imageLab = new ImageLabel(width_, height_);
    curr_image_.create(height_, width_, CV_8UC3);
    
    pitchSlider = new QSlider(Qt::Vertical);
    pitchSlider->setRange(-90, 90);
    yawSlider = new QSlider(Qt::Horizontal);
    yawSlider->setRange(-90, 90);
    
    btnWR = new QPushButton("Walk Remote");
    
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(imageLab);
    leftLayout->addWidget(yawSlider);
    
    QHBoxLayout *mainLayout = new QHBoxLayout();
    mainLayout->addLayout(leftLayout);
    mainLayout->addWidget(pitchSlider);
    mainLayout->addWidget(btnWR);
    
    QWidget *mainWidget  = new QWidget();
    mainWidget->setLayout(mainLayout);
    this->setCentralWidget(mainWidget);
    
    yawLab = new QLabel();
    pitchLab = new QLabel();
    netLab = new QLabel();
    netLab->setFixedWidth(100);
    
    statusBar()->addWidget(pitchLab);
    statusBar()->addWidget(yawLab);
    statusBar()->addWidget(netLab);
    
    net_info = QString::fromStdString(CONF->get_config_value<string>(CONF->player()+".address"))
               +":"+ QString::number(CONF->get_config_value<int>("net.tcp.port"));
    setWindowTitle(net_info);

    timer= new QTimer;
    timer->start(1000);

    connect(timer, &QTimer::timeout, this, &image_monitor::procTimer);
    connect(yawSlider, &QSlider::valueChanged, this, &image_monitor::procYawSlider);
    connect(pitchSlider, &QSlider::valueChanged, this, &image_monitor::procPitchSlider);
    connect(btnWR, &QPushButton::clicked, this, &image_monitor::procBtnWR);
    client_.start();
    yawSlider->setEnabled(false);
    pitchSlider->setEnabled(false);
}

void image_monitor::data_handler(const tcp_command cmd)
{
    if(cmd.type == IMAGE_DATA)
    {
        vector<unsigned char> buf(cmd.size);
        memcpy(&buf[0], cmd.data.c_str(), cmd.size);
        Mat bgr = imdecode(buf, cv::IMREAD_COLOR);
        Mat dst;
        cvtColor(bgr, dst, CV_BGR2RGB);
        QImage *disImage = new QImage((const unsigned char*)(dst.data),dst.cols,dst.rows,QImage::Format_RGB888);
        imageLab->setPixmap(QPixmap::fromImage(disImage->scaled(imageLab->size(), Qt::KeepAspectRatio)));
        delete disImage;
        bgr.release();
        dst.release();
    }
}

void image_monitor::procTimer()
{
    if(client_.is_connected())
    {
        if(first_connect)
        {
            client_.regist(IMAGE_DATA, DIR_APPLY);
            usleep(10000);
            client_.regist(REMOTE_DATA, DIR_SUPPLY);
        }
        first_connect = false;
        netLab->setStyleSheet("background-color:green");
        yawSlider->setEnabled(true);
        pitchSlider->setEnabled(true);
    }
    else
    {
        first_connect = true;
        netLab->setStyleSheet("background-color:red");
        yawSlider->setEnabled(false);
        pitchSlider->setEnabled(false);
    }
}

void image_monitor::procBtnWR()
{
    walk_remote *wr = new walk_remote(client_, net_info);
    wr->show();
}

void image_monitor::procPitchSlider(int v)
{
    pitchLab->setText(QString::number(v));
    float yaw = (float)(yawSlider->value());
    float pitch = (float)v;
    remote_data_type t = LOOKAT_DATA;
    
    tcp_command cmd;
    cmd.type = REMOTE_DATA;
    cmd.size = 2*float_size+rmt_type_size;
    cmd.data.clear();
    cmd.data.append((char*)(&t), rmt_type_size);
    cmd.data.append((char*)(&yaw), float_size);
    cmd.data.append((char*)(&pitch), float_size);
    client_.write(cmd);
}

void image_monitor::procYawSlider(int v)
{
    yawLab->setText(QString::number(v));
    float yaw = (float)v;
    float pitch = (float)(pitchSlider->value());
    remote_data_type t = LOOKAT_DATA;
    
    tcp_command cmd;
    cmd.type = REMOTE_DATA;
    cmd.size = 2*float_size+rmt_type_size;
    cmd.data.clear();
    cmd.data.append((char*)(&t), rmt_type_size);
    cmd.data.append((char*)(&yaw), float_size);
    cmd.data.append((char*)(&pitch), float_size);
    client_.write(cmd);
}

void image_monitor::closeEvent(QCloseEvent *event)
{
    client_.stop();
}