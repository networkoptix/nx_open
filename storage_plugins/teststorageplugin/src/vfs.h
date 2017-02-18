#pragma once

#include <string>
#include <functional>
#include "detail/fs_stub.h"

namespace utils {

struct VfsPair
{
    std::string sampleFilePath;
    FsStubNode* root;
};

using GetFileSizeFunc = std::function<int64_t(const char*)>;

bool buildVfsFromJson(
    const char* jsonString, 
    const char* rootPath,
    GetFileSizeFunc getFileSizeFunc, 
    VfsPair* outVfsPair);
}