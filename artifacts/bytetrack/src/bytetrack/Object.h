#pragma once

#include "Rect.h"

namespace byte_track
{
struct Object
{
    Rect<float> rect;
    int label = 0;
    float prob = 1.0;

    Object(const Rect<float> &_rect,
           const int &_label,
           const float &_prob);
};
}