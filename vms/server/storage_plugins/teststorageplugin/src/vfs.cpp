#include <string.h>
#include <time.h>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "vfs.h"
#include "log.h"
#include "detail/json.h"

namespace utils {

namespace {
#if defined(__linux__)
#include <sys/stat.h>

int64_t fileSize(const std::string& fileName)
{
    struct stat st;
    if (stat(fileName.c_str(), &st) != 0)
        return 0;

    return st.st_size;
}

#elif defined (_WIN32)
#include <windows.h>

int64_t fileSize(const std::string& fileName)
{
    TCHAR buf[2048];
    MultiByteToWideChar(CP_UTF8, 0, fileName.c_str(), fileName.size(), buf, 2048);

    HANDLE hFile = CreateFile(buf, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!hFile)
        return 0;

    LARGE_INTEGER result;
    if (!GetFileSizeEx(hFile, &result))
    {
        CloseHandle(hFile);
        return 0;
    }

    CloseHandle(hFile);
    return result.QuadPart;
}
#endif

}

std::string jsonTypeName(enum JsonType type)
{
    switch (type)
    {
    case jsonStringT: return "JsonString";
    case jsonNumberT: return "JsonNumber";
    case jsonObjectT: return "JsonObject";
    case jsonArrayT: return "JsonArray";
    case jsonTrueT:
    case jsonFalseT: return "JsonBoolean";
    case jsonNullT: return "JsonNull";
    }

    return "";
}

bool checkJsonObjValue(
    const struct JsonVal* val, 
    enum JsonType type, 
    const char* key, 
    const char* originalString)
{
    const char* noKeyErrorStr = "[TestStorage, JSON]: no '%s' key found: json string: %s\n";
    const char* wrongTypeErrorStr = "[TestStorage, JSON]: '%s' is not of %s type, actual type is %s: json string: %s\n";

    if (val == nullptr)
    {
        LOG(noKeyErrorStr, key, originalString);
        return false;
    }

    if (val->type != type)
    {
        LOG(wrongTypeErrorStr, key, jsonTypeName(type).c_str(), 
            jsonTypeName(val->type).c_str(), originalString);
        return false;
    }

    return true;
}

std::string fsJoin(const std::string& subPath1, const std::string& subPath2) 
{
    if (subPath1[subPath1.size() - 1] == '/') 
        return subPath1 + subPath2;

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

std::string pathFromTimeStamp(const char* timestampString, const char* durationString)
{
    std::stringstream out;
    time_t t = (time_t)(std::stoll(timestampString) / 1000);
    struct tm *ltime = localtime(&t);

    out.fill('0');
    out << (1900 + ltime->tm_year) << "/"
        << std::setw(2) << (ltime->tm_mon + 1) << "/"
        << std::setw(2) << (ltime->tm_mday) << "/"
        << std::setw(2) << (ltime->tm_hour) << "/"
        << timestampString << "_" << durationString << ".mkv";

    return out.str();
}

bool addChunks(JsonVal* arrayVal, FsStubNode* root,  const std::string& rootPath,
               const char* jsonString, int64_t fileSize)
{
    for (int i = 0; i < JsonVal_arrayLen(arrayVal); ++i)
    {
        JsonVal* chunkVal = JsonVal_arrayAt(arrayVal, i);
        if (!checkJsonObjValue(chunkVal, jsonObjectT, "chunk", jsonString))
            return false;

        const char* kDurationKey = "durationMs";
        JsonVal* durationVal = JsonVal_getObjectValueByKey(chunkVal, kDurationKey);
        if (!checkJsonObjValue(durationVal, jsonStringT, kDurationKey, jsonString))
            return false;

        const char* kStartTimeKey = "startTimeMs";
        JsonVal* startTimeVal = JsonVal_getObjectValueByKey(chunkVal, kStartTimeKey);
        if (!checkJsonObjValue(startTimeVal, jsonStringT, kStartTimeKey, jsonString))
            return false;

        FsStubNode_add(
            root, 
            fsJoin(rootPath,
                   pathFromTimeStamp(startTimeVal->u.string,
                                     durationVal->u.string)).c_str(),
            file,
            660,
            fileSize);
    }

    return true;
}

}

bool utils::buildVfsFromJson(const char* jsonString, const char* rootPath, VfsPair* outVfsPair)
{
    char errorBuf[512];
    JsonVal rootObj = jsonParseString(jsonString, errorBuf, sizeof(errorBuf));
    JsonValAutoDestroy rootWrapper(&rootObj);
    (void)rootWrapper;

    if (*errorBuf != 0)
    {
        LOG("[TestStorage, JSON]: failed to parse json string: %s\n", jsonString);
        return false;
    }

    /* parse top level object */
    /* sample file */
    const char* kSampleKey = "sample";
    auto sampleVal = JsonVal_getObjectValueByKey(&rootObj, kSampleKey);
    if (!checkJsonObjValue(sampleVal, jsonStringT, kSampleKey, jsonString))
        return false;
    outVfsPair->sampleFilePath = sampleVal->u.string;
    outVfsPair->sampleFileSize = fileSize(outVfsPair->sampleFilePath);

    /* cameras */
    const char* kCamerasKey = "cameras";
    auto camerasArrayVal = JsonVal_getObjectValueByKey(&rootObj, kCamerasKey);
    if (!checkJsonObjValue(camerasArrayVal, jsonArrayT, kCamerasKey, jsonString))
        return false;

    /* here we should already create VFS root */
    outVfsPair->root = fsStubCreateTopLevel();

    /* for each camera */
    for (int i = 0; i < JsonVal_arrayLen(camerasArrayVal); ++i)
    {
        auto cameraVal = JsonVal_arrayAt(camerasArrayVal, i);
        if (!checkJsonObjValue(cameraVal, jsonObjectT, "camera", jsonString))
            return false;

        /* camera id */
        const char* kIdKey = "id";
        auto idVal = JsonVal_getObjectValueByKey(cameraVal, kIdKey);
        if (!checkJsonObjValue(idVal, jsonStringT, kIdKey, jsonString))
            return false;

        /* hi quality chunks for the given camera */
        const char* kHiQualityKey = "hi";
        auto qualityVal = JsonVal_getObjectValueByKey(cameraVal, kHiQualityKey);
        if (!checkJsonObjValue(qualityVal, jsonArrayT, kHiQualityKey, jsonString))
            return false;

        if (!addChunks(qualityVal,
                       outVfsPair->root,
                       fsJoin(fsJoin(rootPath, "hi_quality"), idVal->u.string),
                       jsonString,
                       outVfsPair->sampleFileSize))
        {
            return false;
        }

        /* low quality chunks for the given camera */
        const char* kLowQualityKey = "low";
        qualityVal = JsonVal_getObjectValueByKey(cameraVal, kLowQualityKey);
        if (!checkJsonObjValue(qualityVal, jsonArrayT, kLowQualityKey, jsonString))
            return false;

        if (!addChunks(qualityVal,
                       outVfsPair->root,
                       fsJoin(fsJoin(rootPath, "low_quality"), idVal->u.string),
                       jsonString,
                       outVfsPair->sampleFileSize))
        {
            return false;
        }
    }

    return true;
}
