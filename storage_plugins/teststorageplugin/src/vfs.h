#pragma once

#include <string>
#include "detail/fs_stub.h"

namespace utils {

FsStubNode* buildVfsFromJson(const char* jsonString);
}