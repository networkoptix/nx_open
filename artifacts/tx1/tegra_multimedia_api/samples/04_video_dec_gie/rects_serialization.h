#pragma once

#include <string>
#include <iostream>
#include <vector>

#include "tegra_video.h"

bool writeRectsToFile(
    const std::string& filename, const TegraVideo::Rect rects[], int rectCount);

bool readRectsFromFile(
    const std::string& filename, TegraVideo::Rect outRects[], int maxRectCount, int* outRectCount);
