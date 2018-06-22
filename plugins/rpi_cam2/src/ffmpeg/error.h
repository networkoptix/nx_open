#pragma once

#include <string>

namespace nx{
namespace webcam_plugin{
namespace ffmpeg {
namespace error {

std::string avStrError(int errorCode);
bool updateIfError(int errorCode);
void setLastError(int errorCode);
int lastError();
bool hasError();

} // namespace error
} // namespace ffmpeg
} // namespace webcam_plugin
} // namespace nx