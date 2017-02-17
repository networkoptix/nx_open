#include "vfs.h"
#include "detail/json.h"

namespace utils {

FsStubNode* buildVfsFromJson(const char* jsonString)
{
    char errorBuf[512];
    JsonVal* result;

    result = jsonParseString(jsonString, errorBuf, sizeof(errorBuf));
    if (*errorBuf != 0)
        
}
   
}