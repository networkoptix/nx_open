#pragma once

#include <string>

namespace nx{
namespace ffmpeg {
namespace error {

std::string toString(int errorCode);
bool updateIfError(int errorCode);
void setLastError(int errorCode);
int lastError();
bool hasError();

} // namespace error
} // namespace ffmpeg
} // namespace nx