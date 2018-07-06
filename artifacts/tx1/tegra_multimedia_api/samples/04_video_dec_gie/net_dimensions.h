#pragma once

#include <string>
#include <iostream>

struct NetDimensions
{
    NetDimensions(int width, int height): width(width), height(height) {}

    bool isNull() const { return width == 0 && height == 0; }

    int width;
    int height;
};

/**
 * If specifiedDimensions are both > 0, return them as is, otherwise, try to parse the values from
 * "deployFile.prototxt" file. Typical examples of the lines of interest in such files:
 *
 * <code><pre>
 *     dim: 1
 *     dim: 3
 *     dim: 512
 *     dim: 1024
 * </pre></code>
 *
 * <code><pre>
 *     input_dim: 1
 *     input_dim: 3
 *     input_dim: 540
 *     input_dim: 960
 * </pre></code>
 *
 * @return {0, 0} if attempted but unable to parse either width or height.
 */
NetDimensions getNetDimensions(
    std::istream& input, const std::string& filename, NetDimensions specifiedDimensions);
