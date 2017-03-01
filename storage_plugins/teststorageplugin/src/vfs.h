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

bool buildVfsFromJson(const char* jsonString, const char* rootPath, VfsPair* outVfsPair);
}