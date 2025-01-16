// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <camera/camera_plugin.h>

void logCameraInfo(const nxcip::CameraInfo& info, const std::string& label);

/**
 * Extracts the path from a URL like `file:///`; remove the trailing `/` or `\`, if any. If the URL
 * does not contain the third slash, ignore this fact. If the URL does not have the expected scheme
 * prefix, returns it as is.
 */
std::string fileUrlToPath(const std::string& url);
