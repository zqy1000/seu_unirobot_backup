#pragma once

#include <string>

struct filed_info
{
    int field_length;
    int field_width;
    int goal_depth;
    int goal_width;
    int goal_height;
    int goal_area_length;
    int goal_area_width;
    int penalty_mark_distance;
    int center_circle_diameter;
    int border_strip_width_min;
};

struct player_info
{
    int id;
    float x, y, dir;
    float ball_x, ball_y;
};

struct camera_para
{
    std::string name;
    int id;
    bool sw;
    float value;
    float default_value;
    float min_value;
    float max_value;
};

enum FallDirection
{
    FALL_NONE = 0,
    FALL_FORWARD = 1,
    FALL_BACKWARD = -1,
    FALL_LEFT = 2,
    FALL_RIGHT = -2
};

enum { player_info_size = sizeof(player_info)};
