#include <string.h>
#include <time.h>
#include <unordered_map>
#include "vfs.h"
#include "log.h"
#include "detail/json.h"

namespace utils {

void setNodeSize(void* ctx, struct FsStubNode* fsNode)
{
    int64_t* fileSize = (int64_t*)ctx;
    if (fsNode->type == file)
        fsNode->size = *fileSize;
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

const char* jsonTypeName(enum JsonType type, char* buf, int bufLen)
{
#define COPYNAME(name) strncpy(buf, #name, MIN(bufLen, strlen(#name) + 1)) 
    switch (type)
    {
    case jsonStringT:
        COPYNAME(string);
        break;
    case jsonNumberT:
        COPYNAME(number);
        break;
    case jsonObjectT:
        COPYNAME(object);
        break;
    case jsonArrayT:
        COPYNAME(array);
        break;
    case jsonTrueT:
    case jsonFalseT:
        COPYNAME(boolean)
        break;
    case jsonNullT:
        COPYNAME(null);
        break;
    }
#undef COPYNAME
}

#undef MIN

bool checkJsonObjValue(
    const struct JsonVal* val, 
    enum JsonType type, 
    const char* key, 
    const char* originalString)
{
    const char* noKeyErrorStr = "[TestStorage, JSON]: no '%s' key found: json string: %s\n";
    const char* wrongTypeErrorStr = "[TestStorage, JSON]: '%s' is not of %s type, actual type is %s: json string: %s\n";
    constexpr int kBufLen;
    char buf[kBufLen];

    if (val == nullptr)
    {
        LOG(noKeyErrorStr, key, originalString);
        return false;
    }

    if (val->type != type)
    {
        LOG(wrongTypeErrorStr, key, jsonTypeName(type, buf, kBufLen), 
            jsonTypeName(val->type, buf, kBufLen), originalString);
        return false;
    }

    return true;
}

std::string fsJoin(const std::string& subPath1, const std::string& subPath2) 
{
    if (subPath1[subPath1.size() - 1] == '/') 
        return subPath1 + subPath2)

    return subPath1 + "/" + subPath2;
}

struct JsonValAutoDestroy
{
    JsonValAutoDestroy(JsonVal* jsonVal) : jsonVal(jsonVal) {}
    ~JsonValAutoDestroy()
    {
        if (jsonVal)
            JsonVal_destroy(jsonVal);
    }
    JsonVal* jsonVal;
};

}

/*
Json object should have the following structure:
{
    "sample": "/path/to/sample/file",
    "cameras": [
        {
            "id": "someCameraId",
            "chunks": {
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
        }
    ]
}
*/

bool buildVfsFromJson(
    const char* jsonString, 
    const char* rootPath, 
    GetFileSizeFunc getFileSizeFunc, 
    VfsPair* outVfsPair)
{
    char errorBuf[512];
    JsonVal rootObj;
    JsonVal* curVal, *cameraVal, *idVal, *qualityVal, *chunksVal;
    int camerasCount;
    int chunksCount;

    rootObj = jsonParseString(jsonString, errorBuf, sizeof(errorBuf));
    JsonValAutoDestroy rootWrapper(&rootObj);
    (void)rootWrapper;

    if (*errorBuf != 0)
    {
        LOG("%s: failed to parse json string: %s\n", __FUNCTION__, jsonString);
        return false;
    }

    /* parse top level object */
    /* sample file */
    const char* kSampleKey = "sample";
    curVal = JsonVal_getObjectValueByKey(&rootObj, kSampleKey);
    if (!checkJsonObjValue(curVal, jsonStringT, kSampleKey, jsonString))
        return false;

    /* cameras */
    const char* kCamerasKey = "cameras";
    curVal = JsonVal_getObjectValueByKey(&rootObj, kCamerasKey);
    if (!checkJsonObjValue(curVal, jsonArrayT, kCamerasKey, jsonString))
        return false;

    /* here we should already create VFS root */
    outVfsPair->root = fsStubCreateTopLevel();

    /* for each camera */
    for (int i = 0; i < JsonVal_arrayLen(curVal); ++i)
    {
        cameraVal = JsonVal_arrayAt(curVal, i);
        if (!checkJsonObjValue(cameraVal, jsonObjectT, "camera", jsonString))
            return false;

        /* camera id */
        const char* kIdKey = "id";
        idVal = JsonVal_getObjectValueByKey(cameraVal, idKey);
        if (!checkJsonObjValue(idVal, jsonStringT, kIdKey, jsonString))
            return false;

        /* hi quality chunks for the given camera */
        const char* kHiQualityKey = "hi";
        qualityVal = JsonVal_getObjectValueByKey(cameraVal, kHiQualityKey);
        if (!checkJsonObjValue(qualityVal, jsonArrayT, kHiQualityKey, jsonString))
            return false;

        if (!addChunks(qualityVal, 
                &rootObj, 
                fsJoin(fsJoin(rootPath, "hi_quality"), cameraVal->u.string)))
        {
            return false;
        }
    }

    FsStubNode_forEach(result.vfsPair->root, &fileSize, &setNodeSize);

    JsonVal_destroy(&rootObj);
    return true;
}
   
}