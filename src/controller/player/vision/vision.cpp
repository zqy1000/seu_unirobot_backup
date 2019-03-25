#include "vision.hpp"
#include "parser/camera_parser.hpp"
#include "darknet/parser.h"
#include <cuda_runtime.h>
#include "cuda/cudaproc.h"
#include "server/server.hpp"
#include <algorithm>
#include "compare.hpp"
#include "core/worldmodel.hpp"
#include "imageproc/imageproc.hpp"
#include <fstream>

using namespace std;
using namespace cv;
using namespace robot_math;
using namespace Eigen;
using namespace vision;
using namespace robot;

Vision::Vision(): timer(CONF->get_config_value<int>("vision_period"))
{
    p_count_ = 0;
    cant_see_ball_count_ = 0;
    is_busy_ = false;
    w_ = CONF->get_config_value<int>("image.width");
    h_ = CONF->get_config_value<int>("image.height");
    img_sd_type_ = IMAGE_SEND_RESULT;
    camera_src_ = nullptr;
    detector_ = make_shared<Detector>(w_, h_);
    parser::camera_info_parser::parse(CONF->get_config_value<string>(CONF->player() + ".camera_info_file"), camera_infos_);
    parser::camera_param_parser::parse(CONF->get_config_value<string>(CONF->player() + ".camera_params_file"), params_);
    LOG << setw(12) << "algorithm:" << setw(18) << "[vision]" << " started!" << ENDL;
    ball_ = object_prob(0, CONF->get_config_value<float>("detection_prob.ball"));
    post_ = object_prob(1, CONF->get_config_value<float>("detection_prob.post"));
    dets_prob_[0] = CONF->get_config_value<float>("detection_prob.ball");
    dets_prob_[1] = CONF->get_config_value<float>("detection_prob.post");

}

Vision::~Vision()
{
    LOG << setw(12) << "algorithm:" << setw(18) << "[vision]" << " ended!" << ENDL;
}

void Vision::set_camera_info(const camera_info &para)
{
    for (auto &item : camera_infos_)
    {
        if (item.second.id == para.id)
        {
            item.second.value = para.value;
            break;
        }
    }
}

Vector2d Vision::odometry(const Vector2i &pos, const Eigen::Vector3d &vec)
{
    float Xw, Yw;
    float OC = vec[0];
    float roll = static_cast<float>(vec[1]);
    float theta = static_cast<float>(vec[2]);
    Vector2f centerPos(pos.x()*2 - params_.cx, params_.cy - pos.y()*2);
    Vector2i calCenterPos(static_cast<int>(centerPos.x()/cos(roll)), 
            static_cast<int>(centerPos.y()+centerPos.x()*tan(roll)));
    Vector2i calPos(calCenterPos.x() + params_.cx, params_.cy - calCenterPos.y());
    double gama = atan((params_.cy-(float)calPos.y())/params_.fy);
    double O_C_P = M_PI_2-theta+gama;
    Yw = OC*tan(O_C_P);
    Xw = ((float)calPos.x()-params_.cx)*OC*cos(gama)/(cos(O_C_P)*params_.fx);
    return Vector2d(Xw, Yw);
}

void Vision::run()
{
    if (is_alive_)
    {

        if (camera_src_ == nullptr)
        {
            return;
        }

        p_count_ ++;

        if (is_busy_)
        {
            return;
        }

        Vector3d camera_vec;
        float head_yaw;
        double t1 = clock();
        cudaError_t err;
        frame_mtx_.lock();
        err = cudaMemcpy(dev_src_, camera_src_, src_size_, cudaMemcpyHostToDevice);
        check_error(err);
        if(OPTS->use_robot())
        {
            camera_vec = camera_vec_;
            head_yaw = head_yaw_;
        }
        frame_mtx_.unlock();
        
        if (use_mv_)
        {
            cudaBayer2BGR(dev_src_, dev_bgr_,  camera_w_,  camera_h_, camera_infos_["saturation"].value,
                        camera_infos_["red_gain"].value, camera_infos_["green_gain"].value, camera_infos_["blue_gain"].value);
            cudaUndistored(dev_bgr_, dev_undistored_, camera_w_, camera_h_, params_.fx, params_.fy, params_.cx, params_.cy,
                        params_.k1, params_.k2, params_.p1, params_.p2);
            cudaResizePacked(dev_undistored_, camera_w_,  camera_h_,  dev_ori_, w_,  h_);
        }
        else
        {
            cudaYUYV2BGR(dev_src_,  dev_bgr_,  camera_w_,  camera_h_);
            cudaResizePacked(dev_bgr_, camera_w_,  camera_h_,  dev_ori_, w_,  h_);
        }
        //cudaBGR2YUV422(dev_ori_, dev_yuyv_, w_, h_);
        cudaResizePacked(dev_ori_, w_, h_, dev_sized_, net_.w, net_.h);
        cudaBGR2RGBfp(dev_sized_, dev_rgbfp_, net_.w, net_.h);

        is_busy_ = true;

        //detector_->process(dev_yuyv_);
        layer l = net_.layers[net_.n - 1];
        //double t1 = clock();
        network_predict(net_, dev_rgbfp_, 0);
        int nboxes = 0;
        float nms = .45;
        detection *dets = get_network_boxes(&net_, w_, h_, 0.5, 0.5, 0, 1, &nboxes, 0);

        if (nms)
        {
            do_nms_sort(dets, nboxes, l.classes, nms);
        }
        ball_dets_.clear();
        post_dets_.clear();
        for (int i = 0; i < nboxes; i++)
        {
            if(dets[i].prob[0] > dets[i].prob[1])
            {
                detection d = dets[i];
                d.prob[0] = dets[i].prob[0];
                if(d.prob[0] > dets_prob_[0])
                    ball_dets_.push_back(d);
            }
            else
            {
                detection d = dets[i];
                d.prob[0] = dets[i].prob[1];
                if(d.prob[0] > dets_prob_[1])
                    post_dets_.push_back(d);
            }
        }
        sort(ball_dets_.begin(), ball_dets_.end(), CompareDetGreater);
        sort(post_dets_.begin(), post_dets_.end(), CompareDetGreater);

        //vector<object_prob> res;
        //res = ball_and_post_detection(net_, dev_rgbfp_, true, ball_, post_, w_, h_);
        /*
        for(auto &r:res)
        {
            if(OPTS->use_robot())
            {
                Vector2d ball_pos;
                bool find_ball=false;
                vector<Vector2d> post_pos;
                Vector2d odo_res = odometry(Vector2i(r.x, r.y), camera_vec);
                Vector2d odo_cen = odometry(Vector2i(w_/2, h_/2), camera_vec);
                //LOG << odo_res[0] << '\t' << odo_res[1] << "\tball: "<<odo_res.norm()<<ENDL;
                Vector2d obj_pos = rotation_mat_2d(-head_yaw)*odo_res;
                if(r.id == ball_.id)
                {
                    find_ball = true;
                    ball_pos = obj_pos;
                }
                else
                    post_pos.push_back(obj_pos);
                //selflocation
                player_info p = WM->my_info();
                if(find_ball)
                {
                    cant_see_ball_count_=0;
                    Vector2d temp_ball = Vector2d(p.x, p.y)+rotation_mat_2d(-p.dir)*ball_pos;
                    int tempx=r.x-w_/2, tempy=r.y-h_/2;
                    WM->set_ball_pos(temp_ball, ball_pos, Vector2d(tempx/(float)w_*params_.h_v, tempy/(float)h_*params_.v_v));
                }
                else
                {
                    cant_see_ball_count_++;
                    if(cant_see_ball_count_*period_ms_>10000)
                        WM->set_ball_pos(Vector2d(0,0), Vector2d(0,0), Vector2d(0,0), false);
                }
            }
        }
        */

        if (OPTS->use_debug())
        {
            Mat bgr(h_, w_, CV_8UC3);
            err = cudaMemcpy(bgr.data, dev_ori_, ori_size_, cudaMemcpyDeviceToHost);
            check_error(err);
            if(OPTS->image_record()&&p_count_%20==0)
            {
                string name = get_time();
                imwrite(String(name+".png"), bgr);
                ofstream out(name);
                out<<camera_vec;
                out.close();
            }

            if (img_sd_type_ == IMAGE_SEND_ORIGIN)
            {
                send_image(bgr);
            }
            else if (img_sd_type_ == IMAGE_SEND_RESULT)
            {
                if(!ball_dets_.empty())
                {
                    rectangle(bgr, Point((ball_dets_[0].bbox.x - ball_dets_[0].bbox.w / 2.0)*w_, (ball_dets_[0].bbox.y - ball_dets_[0].bbox.h / 2.0)*h_),
                              Point((ball_dets_[0].bbox.x + ball_dets_[0].bbox.w / 2.0)*w_, (ball_dets_[0].bbox.y + ball_dets_[0].bbox.h / 2.0)*h_),
                              Scalar(255, 0, 0, 0), 2);
                    putText(bgr, to_string(ball_dets_[0].prob[0]).substr(0,4), Point(ball_dets_[0].bbox.x * w_-40, ball_dets_[0].bbox.y * h_-40),
                                    FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 0, 0), 2, 8);
                }
                int i=0;
                for(auto &dd: post_dets_)
                {
                    if(i>=2) break;
                    rectangle(bgr, Point((post_dets_[i].bbox.x - post_dets_[i].bbox.w / 2.0)*w_, (post_dets_[i].bbox.y - post_dets_[i].bbox.h / 2.0)*h_),
                              Point((post_dets_[i].bbox.x + post_dets_[i].bbox.w / 2.0)*w_, (post_dets_[i].bbox.y + post_dets_[i].bbox.h / 2.0)*h_),
                              Scalar(0, 0, 255, 0), 2);
                    putText(bgr, to_string(post_dets_[i].prob[0]).substr(0,4), Point(post_dets_[i].bbox.x * w_-40, post_dets_[i].bbox.y * h_-40),
                                    FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 0, 255), 2, 8);
                    i++;
                }

                /*
                for(int j=0;j<h_;j++)
                {
                    for(int i=0;i<w_;i++)
                    {
                        if(detector_->isGreen(i, j))
                        {
                            circle(bgr, Point(i, j), 1, Scalar(255,0,0), -1);
                        }
                    }
                }
                
                for(int i=0; i<res.size();i++)
                {
                    Scalar clr = res[i].id == ball_.id ? Scalar(255,0,0):Scalar(0,0,255);
                    circle(bgr, Point(res[i].x, res[i].y), 10, clr, -1);
                }*/
                send_image(bgr);
            }
        }
        is_busy_ = false;

    }
}

void Vision::send_image(const cv::Mat &src)
{
    cv::Mat bgr;
    src.copyTo(bgr);
    std::vector<unsigned char> jpgbuf;
    cv::imencode(".jpg", bgr, jpgbuf);
    bgr.release();
    tcp_command cmd;
    cmd.type = IMG_DATA;
    cmd.size = jpgbuf.size();
    cmd.data.assign((char *) & (jpgbuf[0]), jpgbuf.size());
    SERVER->write(cmd);
}

void Vision::updata(const pub_ptr &pub, const int &type)
{

    if (!is_alive_)
    {
        return;
    }
    if (type == sensor::SENSOR_CAMERA)
    {
        shared_ptr<camera> sptr = dynamic_pointer_cast<camera>(pub);

        if (camera_src_ ==  nullptr)
        {
            camera_w_ = sptr->camera_w();
            camera_h_ = sptr->camera_h();
            camera_size_ = sptr->camera_size();
            use_mv_ = sptr->use_mv();
            src_size_ = camera_size_;
            bgr_size_ = camera_w_ * camera_h_ * 3;
            camera_src_ = (unsigned char *)malloc(camera_size_);
            cudaError_t err;
            err = cudaMalloc((void **) &dev_src_,  src_size_);
            check_error(err);
            err = cudaMalloc((void **) &dev_bgr_, bgr_size_);
            check_error(err);
            err = cudaMalloc((void **) &dev_undistored_, bgr_size_);
            check_error(err);
        }
        frame_mtx_.lock();
        memcpy(camera_src_, sptr->buffer(), src_size_);
        if(OPTS->use_robot())
        {
            AngleAxisd roll, pitch;
            pitch = AngleAxisd(deg2rad(imu_data_.pitch), Vector3d::UnitY());
            roll = AngleAxisd(deg2rad(imu_data_.roll), Vector3d::UnitX());
            Quaternion<double> quat = roll * pitch;
            int spf = WM->support_foot();
            std::vector<double> foot_degs = ROBOT->get_foot_degs(spf);
            std::vector<double> head_degs = ROBOT->get_head_degs();
            transform_matrix body = ROBOT->leg_forward_kinematics(foot_degs, spf);
            body.set_R(quat.matrix());
            robot_math::transform_matrix camera_matrix;
            camera_matrix = body*transform_matrix(0,0,ROBOT->trunk_length())*transform_matrix(head_degs[0],'z')
                            *transform_matrix(0, 0, ROBOT->neck_length())*transform_matrix(head_degs[1], 'y')
                            *transform_matrix(0,0,ROBOT->head_length());
            head_yaw_  = head_degs[0];
            camera_vec_ = Vector3d(camera_matrix.p().z(), deg2rad(imu_data_.roll), deg2rad(imu_data_.pitch+head_degs[1]));
        }
        frame_mtx_.unlock();
        return;
    }
    if(type == sensor::SENSOR_IMU)
    {
        shared_ptr<imu> sptr = dynamic_pointer_cast<imu>(pub);
        imu_mtx_.lock();
        imu_data_ = sptr->data();
        imu_mtx_.unlock();
        return;
    }
}

bool Vision::start()
{
    names_.clear();
    ifstream ifs(CONF->get_config_value<string>("net_names_file"));

    while (!ifs.eof())
    {
        string s;
        ifs >> s;
        names_.push_back(s);
    }

    ifs.close();
    net_.gpu_index = 0;
    net_ = parse_network_cfg_custom((char *)CONF->get_config_value<string>("net_cfg_file").c_str(), 1);
    load_weights(&net_, (char *)CONF->get_config_value<string>("net_weights_file").c_str());
    set_batch_network(&net_, 1);
    fuse_conv_batchnorm(net_);
    calculate_binary_weights(net_);
    srand(2222222);

    ori_size_ = w_ * h_ * 3;
    yuyv_size_ = w_*h_*2;
    sized_size_ = net_.w * net_.h * 3;
    rgbf_size_ = w_ * h_ * 3 * sizeof(float);

    cudaError_t err;
    err = cudaMalloc((void **)&dev_ori_, ori_size_);
    check_error(err);
    err = cudaMalloc((void**)&dev_yuyv_, yuyv_size_);
    check_error(err);
    err = cudaMalloc((void **)&dev_sized_, sized_size_);
    check_error(err);
    err = cudaMalloc((void **)&dev_rgbfp_, rgbf_size_);
    check_error(err);
    is_alive_ = true;
    start_timer();
    return true;
}

void Vision::stop()
{
    if (is_alive_)
    {
        delete_timer();
        free_network(net_);
        free(camera_src_);
        cudaFree(dev_ori_);
        cudaFree(dev_yuyv_);
        cudaFree(dev_src_);
        cudaFree(dev_bgr_);
        cudaFree(dev_undistored_);
        cudaFree(dev_rgbfp_);
        cudaFree(dev_sized_);
    }

    is_alive_ = false;
}
