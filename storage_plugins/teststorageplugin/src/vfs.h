#pragma once

#include <string>
#include "detail/fs_stub.h"

namespace utils {

struct VfsPair
{
    std::string sampleFilePath;
    FsStubNode* root;
};

bool buildVfsFromJson(
    const char* jsonString, 
    const char* rootPath, 
    VfsPair* outVfsPair);
}