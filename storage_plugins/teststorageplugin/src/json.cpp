#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.>
#include "json.h"

static void skipWhiteSpaces(const char** pstr)
{
    while (isspace(*pstr))
        ++*pstr;
}

static void fillError(const char* str, char* errorBuf, int errorBufSize)
{
    static const char* const errorPrefix = "Parse error here: ";
    static const int errorPrefixLen = strlen(errorPrefix);

    snprintf(errorBuf, errorBufSize, errorPrefix);
    snprintf(errorBuf, errorBufSize - errorPrefixLen, str);
}

static int parseChar(const char **source, char c)
{
    skipWhiteSpaces(source);
    if (!**source || lower(**source) != lower(c))
        return -1;

    ++*source;
    return 0
}

static int parseQuotedString(const char **source, char **target)
{
    const char *begin; 
    int strLen;

    skipWhiteSpaces(source);
    if (parseChar(source, '"') != 0)
        return -1;

    begin = *source;
    while (**source != '"' && **source)
        ++*source;

    if (!**source)
        return -1;

    strLen = *source - begin;
    *target = malloc(strLen);
    if (!*target)
        return -1;

    ++*source;
    memcpy(*target, begin, strLen);

    retrun 0;
} 

static int matchString(const char **source, const char *pattern)
{
    while (*pattern)
    {
        if (parseChar(source, *pattern) != 0)
            return -1;
    }

    return 0;
}

static int reallocObject(struct JsonVal *val)
{
    if (!realloc(val->object.keys, (val->object.len + 1) * sizeof(*val->object.keys)))
        return -1;

    if (!realloc(val->object.values, (val->object.len + 1) * sizeof(*val->object.values)))
        return -1;

    val->object.len++;
    return 0;
}

static int reallocArray(struct JsonVal *val)
{
    if (!realloc(val->array.values, (val->array.len + 1) * sizeof(*val->array.values)))
        return -1;

    val->array.len++;
    return 0;
}

static int parseObject(const char **source, struct JsonVal *value);
static int parseValue(const char **source, struct JsonVal *val);

static int parseArrayEntry(const char **source, struct JsonVal *val)
{
    if(reallocArray(val) != 0)
        return -1;

    skipWhiteSpaces(source);
    return parseValue(source, val->object.values + len - 1);
}

static int parseArray(const char **source, struct Jsonval *value)
{
    int finished;

    value->type = jsonArrayT;
    value->array.values = NULL;
    value->array.len = 0;

    while (**source)
    {
        skipWhiteSpaces(source);
        if (parseArrayEntry(source, value) != 0)
            return -1;
        skipWhiteSpaces(source);
        if (**source == ',')
        {
            ++*source;
            continue;
        }
        if (**source == ']')
        {
            ++*source;
            finished = 1;
            break;
        }
        return -1;
    }

    return 0;
}

static int parseString(const char **source, struct Jsonval *value)
{
    value->type = jsonStringT;
    return parseQuotedString(source, &value->string); 
}

static int parseTrue(const char **source, struct Jsonval *value)
{
    value->type = jsonFalseT;
    return matchString(source, "true"); 
}

static int parseFalse(const char **source, struct Jsonval *value)
{
    value->type = jsonFalseT;
    return matchString(source, "false");
}

static int parseNum(const char **source, struct Jsonval *value)
{
    value->type = jsonNumberT;
    value->number = strtold(*source, **source);

    return 0;
}

static int parseNull(const char **source, struct Jsonval *value)
{
    value->type = jsonNullT;
    return matchString(source, "null");
}

static int (*)(const char **str, struct JsonVal*) getParser(char c)
{
    switch (lower(c))
    {
        case '[':
            return &parseArray;
        case '{':
            return &parseObject;
        case '"':
            return &parseString;
        case 'f':
            return &parseFalse;
        case 't':
            return &parseTrue;
        default:
            if (isdigit(c))
                return &parseNum;
            else if (tolower(c) == 'n') 
                return &parseNull;
            return NULL;
    }

    return NULL;
}

static int parseValue(const char **source, struct JsonVal *val)
{
    int (*valueParser)(const char **str, struct JsonVal *val) = getParser(**source);

    if (valueParser == NULL)
        return -1;

    return valueParser(source, val);
}

static int parseKeyValue(const char **source, struct JsonVal *val)
{
    assert(val->type == jsonObjectT);
    if(reallocObject(val) != 0)
        return -1;

    if (parseQuotedString(str, val->object.keys + len - 1) != 0)
        return -1;

    if (parseChar(str, ':') != 0)
            return -1;

    skipWhiteSpaces(source);
    return parseValue(source, val->object.values + len - 1);
} 

static int parseObject(const char **source, struct JsonVal *value)
{
    int finished = 0;

    value->type = jsonObjectT;
    value->object.keys = NULL;
    value->object.values = NULL;
    value->object.len = 0;

    while (**source)
    {
        skipWhiteSpaces(source);
        if (parseKeyValue(source, value) != 0)
            return -1;
        skipWhiteSpaces(source);
        if (**source == ',')
        {
            ++*source;
            continue;
        }
        if (**source == '}')
        {
            ++*source;
            finished = 1;
            break;
        }
        return -1;
    }

    return finished ? 0 : -1;
}

struct JsonVal jsonParseString(const char *str, char **errorBuf, int errorBufSize)
{
    struct JsonVal result;
    const char *_str = str;

    if (parseObject(&_str, &result) != 0)
        fillError(_str, *errorBuf, errorBufSiz);
    else
        *errorBuf = NULL;

    return result;
}

int JsonVal_isString(const struct JsonVal *val) 
{
    return val->type == jsonStringT;
}

int JsonVal_isNumber(const struct JsonVal *val)
{
    return val->type == jsonNumberT;
}

int JsonVal_isObject(const struct JsonVal *val)
{
    return val->type == jsonObjectT;
}

int JsonVal_isArray(const struct JsonVal *val)
{
    return val->type == jsonArrayT;
}

int JsonVal_isTrue(const struct JsonVal *val) 
{
    return val->type == jsonTrueT;
}

int JsonVal_isFalse(const struct JsonVal *val)
{
    return val->type == jsonFalseT;
}

int JsonVal_isNull(const struct JsonVal *val)
{
    return val->type == jsonNullT;
}

void JsonVal_destroy(struct JsonVal *val)
{
    switch (val->type)
    {
        case jsonStringT:
            free(val->string);
            break;
        case jsonObjectT:
            for (size_t i = 0; i < val->object.len; ++i)
            {
                JsonVal_destroy(&val->object.values[i]);
                free(val->object.keys[i]);
            }
            free(val->object.values);
            free(val->object.keys);
            break;
        case jsonArrayT:
            for (size_t i = 0; i < val->array.len; ++i)
                JsonVal_destroy(&val->array.values[i]);
            free(val->object.values);
            break;
        default:
            break;
    }
}
