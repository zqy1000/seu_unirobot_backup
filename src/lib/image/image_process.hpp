#ifndef SEU_UNIROBOT_IMAGE_PROCESS_HPP
#define SEU_UNIROBOT_IMAGE_PROCESS_HPP

#include <memory>
#include <opencv2/opencv.hpp>
#include <linux/videodev2.h>
#include "image_define.hpp"

namespace image
{
    class image_process
    {
    public:
        static bool convert2YUV(VideoFrame_Ptr dst640x480x3, const VideoBuffer *buf640x480, const VideoBufferInfo &info)
        {
            if(info.width!=640 || info.height != 480) return false;
            if(dst640x480x3 == nullptr) return false;
            
            unsigned char *src_ptr, *dst_ptr;
            unsigned short dst_offset, src_offset;
            unsigned short width = 640, heigth = 480;
            
            if(info.format == V4L2_PIX_FMT_YUYV)
            {
                for(unsigned short y=0; y<info.height; y++)
                {
                    src_ptr = (unsigned char*)(buf640x480->start + y*(width*2));
                    dst_ptr = dst640x480x3->start+y*(width*3);
                    for(unsigned short x=0; x<width/2; x++)
                    {
                        dst_offset = x*6;
                        src_offset = x*4;
                        *(dst_ptr+dst_offset+0) = *(src_ptr+src_offset+0);
                        *(dst_ptr+dst_offset+1) = *(src_ptr+src_offset+1);
                        *(dst_ptr+dst_offset+2) = *(src_ptr+src_offset+3);
                        *(dst_ptr+dst_offset+3) = *(src_ptr+src_offset+2);
                        *(dst_ptr+dst_offset+4) = *(src_ptr+src_offset+1);
                        *(dst_ptr+dst_offset+5) = *(src_ptr+src_offset+3);
                    }
                }
                return true;
            }
        }
        static bool buffer2Mat_YUV(cv::Mat &dst640x480x3, const VideoBuffer *buf640x480, const VideoBufferInfo &info)
        {
            if(info.width!=640 || info.height != 480) return false;
            if(dst640x480x3.size().width != 640 || dst640x480x3.size().height != 480) return false;
            
            unsigned char *src_ptr, *dst_ptr;
            unsigned short dst_offset, src_offset;
            unsigned short width = 640, heigth = 480;
            
            if(info.format == V4L2_PIX_FMT_YUYV)
            {
                for(unsigned short y=0; y<info.height; y++)
                {
                    src_ptr = (unsigned char*)(buf640x480->start + y*(width*2));
                    dst_ptr = dst640x480x3.data+y*(width*3);
                    for(unsigned short x=0; x<width/2; x++)
                    {
                        dst_offset = x*6;
                        src_offset = x*4;
                        *(dst_ptr+dst_offset+0) = *(src_ptr+src_offset+0);
                        *(dst_ptr+dst_offset+1) = *(src_ptr+src_offset+1);
                        *(dst_ptr+dst_offset+2) = *(src_ptr+src_offset+3);
                        *(dst_ptr+dst_offset+3) = *(src_ptr+src_offset+2);
                        *(dst_ptr+dst_offset+4) = *(src_ptr+src_offset+1);
                        *(dst_ptr+dst_offset+5) = *(src_ptr+src_offset+3);
                    }
                }
                return true;
            }
        }
    };
}

#endif