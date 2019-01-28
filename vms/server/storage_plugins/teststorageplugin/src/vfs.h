#pragma once

#include <string>
#include <functional>
#include <cstdint>

#include <common.h>
#include "detail/fs_stub.h"

namespace utils {

std::string fsJoin(const std::string& subPath1, const std::string& subPath2);

struct VfsPair
{
    std::string sampleFilePath;
    int64_t sampleFileSize;
    FsStubNode* root;

    VfsPair() : root(nullptr) {}
};

/**
Json object should have the following structure:
{
    "sample": "/path/to/sample/file",
    "cameras": [
        {
            "id": "someCameraId",
            "hi": [
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461075"
                },
                ...
            ],
            "low": [
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461075"
                },
                ...
            ]
        }
    ]
}
*/

NX_TEST_STORAGE_PLUGIN_API bool buildVfsFromJson(const char* jsonString, const char* rootPath, VfsPair* outVfsPair);
}
