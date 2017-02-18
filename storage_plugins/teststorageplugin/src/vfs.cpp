#include <string.h>
#include <unordered_map>
#include "vfs.h"
#include "log.h"
#include "detail/json.h"

namespace utils {

namespace {

struct VfsPairSuccess
{
    VfsPair* vfsPair;
    bool success;
    bool chunksFound;

    VfsPairSuccess(VfsPair* vfsPair) : 
        vfsPair(vfsPair), 
        success(true),
        chunksFound(false) 
    {}
};

struct ChunkSuccess
{
    bool durationFound;
    bool startTimeFound;
    int64_t duration;
    int64_t startTime;

    ChunkSuccess() : 
        durationFound(false),
        startTimeFound(false)
    {}
};

const char* pathFromTimeStamp(int64_t timestamp)
{
    return nullptr;
}

void parseChunkObj(void* ctx, const char* key, const struct JsonVal* value)
{
    ChunkSuccess* result = (ChunkSuccess*)ctx;

    if (strcmp(key, "durationMs") == 0)
    {
        if (!JsonVal_isNumber(value))
        {
            LOG("%s: 'durationMs' should be number: %s\n", __FUNCTION__);
            return;
        }
        result->durationFound = true;
        result->duration = value->u.number;
    }
    else if (strcmp(key, "startTimeMs") == 0)
    {
        if (!JsonVal_isNumber(value))
        {
            LOG("%s: 'startTimeMs' should be number: %s\n", __FUNCTION__);
            return;
        }
        result->startTimeFound = true;
        result->startTime = value->u.number;
    }
}

void processChunk(void* ctx, const struct JsonVal* value)
{
    VfsPairSuccess* result = (VfsPairSuccess*)ctx;
    ChunkSuccess chunkSuccess;

    if (!JsonVal_isObject(value))
    {
        LOG("%s: 'chunk' should be an object, ignoring: %s\n", __FUNCTION__);
        return;
    }

    JsonVal_forEachObjectElement(value, &chunkSuccess, &parseChunkObj);
    if (!chunkSuccess.durationFound || !chunkSuccess.startTimeFound)
    {
        LOG("%s: 'chunk' object should have number 'durationMs' field and 'startTimeMs' field, ignoring: %s\n", 
            __FUNCTION__);
        return;
    }

    FsStubNode_add(
        result->vfsPair->root,
        pathFromTimeStamp(chunkSuccess.startTime), 
        file, 0640, 0);
}

void processRootObj(void* ctx, const char* key, const struct JsonVal* value)
{
    VfsPairSuccess* result = (VfsPairSuccess*)ctx;

    if (strcmp(key, "sample") == 0)
    { 
        if (!JsonVal_isString(value))
        {
            result->success = false;
            LOG("%s: 'sample' file path should be string: %s\n", __FUNCTION__);
            return;
        }
        result->vfsPair->sampleFilePath = value->u.string;
    }
    else if (strcmp(key, "chunks") == 0)
    {
        if (!JsonVal_isArray(value))
        {
            result->success = false;
            LOG("%s: 'chunks' should be array: %s\n", __FUNCTION__);
            return;
        }
        result->chunksFound = true;
        JsonVal_forEachArrayElement(value, &result, &processChunk);
    }
}

void setNodeSize(void* ctx, struct FsStubNode* fsNode)
{
    int64_t* fileSize = (int64_t*)ctx;
    fsNode->size = *fileSize;
}

}

bool buildVfsFromJson(
    const char* jsonString, 
    const char* rootPath, 
    GetFileSizeFunc getFileSizeFunc, 
    VfsPair* outVfsPair)
{
    char errorBuf[512];
    VfsPairSuccess result(outVfsPair);
    JsonVal rootObj;

    rootObj = jsonParseString(jsonString, errorBuf, sizeof(errorBuf));
    if (*errorBuf != 0)
    {
        LOG("%s: failed to parse json string: %s\n", __FUNCTION__, jsonString);
        JsonVal_destroy(&rootObj);
        return false;
    }

    result.vfsPair->root = fsStubCreateTopLevel();
    FsStubNode_add(result.vfsPair->root, rootPath, dir, 0777, 0);

    JsonVal_forEachObjectElement(&rootObj, &result, &processRootObj);
    if (!result.success || !result.chunksFound || result.vfsPair->sampleFilePath.empty())
    {
        LOG("%s: root json object is invalid, json string is: %s\n", 
            __FUNCTION__, jsonString);
        JsonVal_destroy(&rootObj);
        FsStubNode_remove(result.vfsPair->root);
        return false;
    }

    int64_t fileSize = getFileSizeFunc(result.vfsPair->sampleFilePath.c_str());   
    if (fileSize < 0)
    {
        LOG("%s: failed to get file size for sample file: %s\n",
            __FUNCTION__, result.vfsPair->sampleFilePath.c_str());
        JsonVal_destroy(&rootObj);
        return false;
    }

    FsStubNode_forEach(result.vfsPair->root, &fileSize, &setNodeSize);

    JsonVal_destroy(&rootObj);
    return true;
}
   
}