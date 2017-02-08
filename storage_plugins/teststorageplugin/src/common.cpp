#include "common.h"

const char* errorCodeToString(ErrorCode ecode)
{
#define ERROR_CODE_APPLY_TO_STRING(value) case value: return #value;
    switch (ecode)
    {
    ERROR_CODE_LIST(ERROR_CODE_APPLY_TO_STRING)
    }
    return nullptr;
}
