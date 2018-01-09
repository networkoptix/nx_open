#pragma once

#include <string>
#include <iostream>
#include <vector>

#include "tegra_video.h"

/**
 * @return False if the file with the required name already exists. Otherwise, return true even in
 * case of an error, after logging the error message.
 */
bool writeRectsToFile(
    const std::string& filename, const TegraVideo::Rect rects[], int rectCount);

/** @return Success, after logging an error message (if any). */
bool readRectsFromFile(
    const std::string& filename, TegraVideo::Rect outRects[], int maxRectCount, int* outRectCount);

/** @return Success, after logging an error message (if any). */
bool writeModulusToFile(const std::string& filename, int64_t modulus);

/** @return Success, after logging an error message (if any). */
bool readModulusFromFile(const std::string& filename, int64_t *outModulus);
