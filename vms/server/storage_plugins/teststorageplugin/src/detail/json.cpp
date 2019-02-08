#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "json.h"


/*
    read and traverse
*/

static void skipWhiteSpaces(const char** pstr)
{
    while (isspace(**pstr))
        ++*pstr;
}

static void fillError(const char* str, char* errorBuf, int errorBufSize)
{
    static const char errorPrefix[] = "Parse error here: ";
    static const int errorPrefixLen = sizeof(errorPrefix);

    snprintf(errorBuf, errorBufSize, "%s", errorPrefix);
    snprintf(errorBuf + errorPrefixLen - 1, errorBufSize - errorPrefixLen, "%s", str);
}

static int parseChar(const char **source, char c)
{
    skipWhiteSpaces(source);
    if (!**source || tolower(**source) != tolower(c))
        return -1;

    ++*source;
    return 0;
}

static int createStringCopy(const char *source, int sourceLen, char **target)
{
	int i, j;
	int escapedSourceLen;

    if (sourceLen == -1)
        sourceLen = strlen(source);

	escapedSourceLen = sourceLen;
	for (i = 0; i < sourceLen; ++i)
	{
		if (source[i] == '\\' && i != sourceLen - 1 && source[i + 1] == '\\')
			escapedSourceLen--;
	}

    *target = (char*)malloc(escapedSourceLen + 1);

    if (!*target)
        return -1;

	for (i = 0, j = 0; i < sourceLen; ++i, ++j)
	{
		if (source[i] == '\\' && i != sourceLen - 1 && source[i + 1] == '\\')
			++i;
		(*target)[j] = source[i];
	}

    (*target)[escapedSourceLen] = '\0';

    return 0;
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
    if (createStringCopy(begin, strLen, target) != 0)
        return -1;

    ++*source;

    return 0;
} 

static int matchString(const char **source, const char *pattern)
{
    while (*pattern)
    {
        if (parseChar(source, *pattern) != 0)
            return -1;
        ++pattern;
    }

    return 0;
}

static int reallocObject(struct JsonVal *val)
{
    if (!(val->u.object.keys = (char **)realloc(
              val->u.object.keys,
              (val->u.object.len + 1) * sizeof(*val->u.object.keys))))
    {
        return -1;
    }

    if (!(val->u.object.values = (struct JsonVal*)realloc(
              val->u.object.values,
              (val->u.object.len + 1) * sizeof(*val->u.object.values))))
    {
        return -1;
    }

    val->u.object.len++;
    return 0;
}

static int reallocArray(struct JsonVal *val)
{
    if (!(val->u.array.values = (struct JsonVal*)realloc(
              val->u.array.values,
              (val->u.array.len + 1) * sizeof(*val->u.array.values))))
    {
        return -1;
    }

    val->u.array.len++;
    return 0;
}

static int parseObject(const char **source, struct JsonVal *value);
static int parseValue(const char **source, struct JsonVal *val);

static int parseArrayEntry(const char **source, struct JsonVal *val)
{
    if(reallocArray(val) != 0)
        return -1;

    skipWhiteSpaces(source);
    return parseValue(source, val->u.array.values + val->u.array.len - 1);
}

static int parseArray(const char **source, struct JsonVal *value)
{
    int finished = 0;

    value->type = jsonArrayT;
    value->u.array.values = NULL;
    value->u.array.len = 0;

    if (parseChar(source, '[') != 0)
        return -1;

    skipWhiteSpaces(source);
    if (**source == ']')
    {
        ++*source;
        return 0;
    }

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

    return finished ? 0 : -1;
}

static int parseString(const char **source, struct JsonVal *value)
{
    value->type = jsonStringT;
    return parseQuotedString(source, &value->u.string);
}

static int parseTrue(const char **source, struct JsonVal *value)
{
    value->type = jsonTrueT;
    return matchString(source, "true");
}

static int parseFalse(const char **source, struct JsonVal *value)
{
    value->type = jsonFalseT;
    return matchString(source, "false");
}

static int parseNum(const char **source, struct JsonVal *value)
{
    value->type = jsonNumberT;
    value->u.number = strtold(*source, (char**)source);

    return 0;
}

static int parseNull(const char **source, struct JsonVal *value)
{
    value->type = jsonNullT;
    return matchString(source, "null");
}

static int (*getParser(char c))(const char **, struct JsonVal*) 
{
    switch (tolower(c))
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

    if (parseQuotedString(source, val->u.object.keys + val->u.object.len - 1) != 0)
        return -1;

    if (parseChar(source, ':') != 0)
        return -1;

    skipWhiteSpaces(source);
    return parseValue(source, val->u.object.values + val->u.object.len - 1);
} 

static int parseObject(const char **source, struct JsonVal *value)
{
    int finished = 0;

    value->type = jsonObjectT;
    value->u.object.keys = NULL;
    value->u.object.values = NULL;
    value->u.object.len = 0;

    skipWhiteSpaces(source);

    if (parseChar(source, '{') != 0)
        return -1;

    skipWhiteSpaces(source);
    if (**source == '}')
    {
        ++*source;
        return 0;
    }

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

struct JsonVal jsonParseString(const char *str, char *errorBuf, int errorBufSize)
{
    struct JsonVal result;
    const char *_str = str;

    result.type = jsonNullT;
    if (parseObject(&_str, &result) != 0)
        fillError(_str, errorBuf, errorBufSize);
    else
        memset(errorBuf, 0, errorBufSize);

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

int JsonVal_arrayLen(struct JsonVal *val)
{
    assert(val->type == jsonArrayT);
    if (val->type != jsonArrayT)
        return -1;

    return val->u.array.len;
}

struct JsonVal *JsonVal_arrayAt(struct JsonVal *val, int index)
{
    assert(val->type == jsonArrayT);
    if (val->type != jsonArrayT || index >= val->u.array.len)
        return NULL;

    return &val->u.array.values[index];
}

struct JsonVal *JsonVal_getObjectValueByKey(const struct JsonVal *val, const char *key)
{
    int i;

    assert(val->type == jsonObjectT);
    if (val->type != jsonObjectT)
        return NULL;

    for (i = 0; i < val->u.object.len; ++i)
    {
        if (strcmp(key, val->u.object.keys[i]) == 0)
            return &val->u.object.values[i];
    }

    return NULL;
}

void JsonVal_forEachArrayElement(
        const struct JsonVal *val,
        void *ctx,
        void (*action)(void *, const struct JsonVal *))
{
    int i;

    assert(val->type == jsonArrayT);
    if (val->type != jsonArrayT)
        return;

    for (i = 0; i < val->u.array.len; ++i)
        action(ctx, &val->u.array.values[i]);
}

void JsonVal_forEachObjectElement(
        const struct JsonVal *val,
        void *ctx,
        void (*action)(void *, const char *key, const struct JsonVal *))
{
    int i;

    assert(val->type == jsonObjectT);
    if (val->type != jsonObjectT)
        return;

    for (i = 0; i < val->u.object.len; ++i)
        action(ctx, val->u.object.keys[i], &val->u.object.values[i]);
}

struct JsonVal jsonCreateObject()
{
    struct JsonVal result;

    result.type = jsonObjectT;
    memset(&result.u, 0, sizeof(result.u));
    return result;
}

static struct JsonVal *prepareNewObjectVal(struct JsonVal *val, const char *key)
{
    struct JsonVal *newValPtr;

    assert(val->type == jsonObjectT);
    if (val->type != jsonObjectT)
        return NULL;

    if (reallocObject(val) != 0)
        return NULL;

    if (createStringCopy(key, strlen(key), val->u.object.keys + val->u.object.len - 1) != 0)
        return NULL;

    newValPtr = val->u.object.values + val->u.object.len - 1;
    memset(newValPtr, 0, sizeof(*newValPtr));

    return newValPtr;
}

struct JsonVal *JsonVal_objectAddString(struct JsonVal *val, const char *key, const char *value)
{
    struct JsonVal *newValPtr = prepareNewObjectVal(val, key);

    if (newValPtr == NULL)
        return NULL;

    if (createStringCopy(value, strlen(value), &newValPtr->u.string) != 0)
        return NULL;

    newValPtr->type = jsonStringT;

    return newValPtr;
}

struct JsonVal *JsonVal_objectAddNumber(struct JsonVal *val, const char *key, long double number)
{
    struct JsonVal *newValPtr = prepareNewObjectVal(val, key);

    if (newValPtr == NULL)
        return NULL;

    newValPtr->u.number = number;
    newValPtr->type = jsonNumberT;

    return newValPtr;
}

struct JsonVal *JsonVal_objectAddObject(struct JsonVal *val, const char *key)
{
    struct JsonVal *newValPtr = prepareNewObjectVal(val, key);

    if (newValPtr == NULL)
        return NULL;

    newValPtr->type = jsonObjectT;

    return newValPtr;
}

struct JsonVal *JsonVal_objectAddArray(struct JsonVal *val, const char *key)
{
    struct JsonVal *newValPtr = prepareNewObjectVal(val, key);

    if (newValPtr == NULL)
        return NULL;

    newValPtr->type = jsonArrayT;

    return newValPtr;
}

struct JsonVal *JsonVal_objectAddTrue(struct JsonVal *val, const char *key)
{
    struct JsonVal *newValPtr = prepareNewObjectVal(val, key);

    if (newValPtr == NULL)
        return NULL;

    newValPtr->type = jsonTrueT;

    return newValPtr;
}

struct JsonVal *JsonVal_objectAddFalse(struct JsonVal *val, const char *key)
{
    struct JsonVal *newValPtr = prepareNewObjectVal(val, key);

    if (newValPtr == NULL)
        return NULL;

    newValPtr->type = jsonFalseT;

    return newValPtr;
}

struct JsonVal *JsonVal_objectAddNull(struct JsonVal *val, const char *key)
{
    struct JsonVal *newValPtr = prepareNewObjectVal(val, key);

    if (newValPtr == NULL)
        return NULL;

    newValPtr->type = jsonNullT;

    return newValPtr;
}

static struct JsonVal *prepareNewArrayVal(struct JsonVal *val)
{
    struct JsonVal *newValPtr;

    assert(val->type == jsonArrayT);
    if (val->type != jsonArrayT)
        return NULL;

    if (reallocArray(val) != 0)
        return NULL;

    newValPtr = val->u.array.values + val->u.array.len - 1;
    memset(newValPtr, 0, sizeof(*newValPtr));

    return newValPtr;
}

struct JsonVal *JsonVal_arrayAddString(struct JsonVal *val, const char *value)
{
    struct JsonVal *newValPtr = prepareNewArrayVal(val);

    if (newValPtr == NULL)
        return NULL;

    if (createStringCopy(value, strlen(value), &newValPtr->u.string) != 0)
        return NULL;

    newValPtr->type = jsonStringT;

    return newValPtr;
}

struct JsonVal *JsonVal_arrayAddNumber(struct JsonVal *val, long double number)
{
    struct JsonVal *newValPtr = prepareNewArrayVal(val);

    if (newValPtr == NULL)
        return NULL;

    newValPtr->type = jsonNumberT;
    newValPtr->u.number = number;

    return newValPtr;
}

struct JsonVal *JsonVal_arrayAddObject(struct JsonVal *val)
{
    struct JsonVal *newValPtr = prepareNewArrayVal(val);

    if (newValPtr == NULL)
        return NULL;

    newValPtr->type = jsonObjectT;

    return newValPtr;
}

struct JsonVal *JsonVal_arrayAddArray(struct JsonVal *val)
{
    struct JsonVal *newValPtr = prepareNewArrayVal(val);

    if (newValPtr == NULL)
        return NULL;

    newValPtr->type = jsonArrayT;

    return newValPtr;
}

struct JsonVal *JsonVal_arrayAddTrue(struct JsonVal *val)
{
    struct JsonVal *newValPtr = prepareNewArrayVal(val);

    if (newValPtr == NULL)
        return NULL;

    newValPtr->type = jsonTrueT;

    return newValPtr;
}

struct JsonVal *JsonVal_arrayAddFalse(struct JsonVal *val)
{
    struct JsonVal *newValPtr = prepareNewArrayVal(val);

    if (newValPtr == NULL)
        return NULL;

    newValPtr->type = jsonFalseT;

    return newValPtr;
}

struct JsonVal *JsonVal_arrayAddNull(struct JsonVal *val)
{
    struct JsonVal *newValPtr = prepareNewArrayVal(val);

    if (newValPtr == NULL)
        return NULL;

    newValPtr->type = jsonNullT;

    return newValPtr;
}

/*
    write
*/

struct Buf
{
    char *buf;
    int spaceLeft;
};

static const int kWriteBufSize = 1024;

static int writeToBuf(void *ctx, void *source, int len)
{
    struct Buf *target = (struct Buf *)ctx;
    int bytesToWrite = len >= target->spaceLeft ? target->spaceLeft : len;

    memcpy(target->buf, source, bytesToWrite);
    target->spaceLeft -= bytesToWrite;
    target->buf += bytesToWrite;

    return bytesToWrite;
}

static void writeChar(
        char c, void *ctx,
        char *writeBuf,
        int (*writeFunc)(void *ctx, void *buf, int len))
{
    writeBuf[0] = c;
    writeFunc(ctx, writeBuf, 1);
}

static void writeString(
        const char *s, void *ctx,
        char *writeBuf,
        int (*writeFunc)(void *ctx, void *buf, int len))
{
    int writeLen = strlen(s);
    int bytesToWrite;

    while (writeLen > 0)
    {
        bytesToWrite = writeLen < kWriteBufSize ? writeLen : kWriteBufSize;
        writeFunc(ctx, (void *)s, bytesToWrite);
        writeLen -= bytesToWrite;
        s += bytesToWrite;
    }
}

static void writeObject(
        const struct JsonVal *val,
        void *ctx, char *writeBuf,
        int (*writeFunc)(void *ctx, void *buf, int len));

static void writeValue(
        const struct JsonVal *val,
        void *ctx, char *writeBuf,
        int (*writeFunc)(void *ctx, void *buf, int len));

static void writeQuotedString(
        const char *s, void *ctx,
        char *writeBuf,
        int (*writeFunc)(void *ctx, void *buf, int len))
{
    writeBuf[0] = '"';
    writeFunc(ctx, writeBuf, 1);

    writeString(s, ctx, writeBuf, writeFunc);

    writeBuf[0] = '"';
    writeFunc(ctx, writeBuf, 1);
}

static void writeNumber(
        long double num,
        void *ctx, char *writeBuf,
        int (*writeFunc)(void *ctx, void *buf, int len))
{
    char *dot;
    char *end;

    sprintf(writeBuf, "%.5Lf", num);
    dot = strchr(writeBuf, '.');
    end = writeBuf + strlen(writeBuf) - 1;

    for (; end != dot; --end)
        if (*end != '0')
            break;

    if (end == dot)
        *end = '\0';
    else
        *(++end) ='\0';

    writeString(writeBuf, ctx, NULL, writeFunc);
}

static void writeArray(
        const struct JsonVal *val,
        void *ctx, char *writeBuf,
        int (*writeFunc)(void *ctx, void *buf, int len))
{
    int i;

    writeChar('[', ctx, writeBuf, writeFunc);

    for (i = 0; i < val->u.array.len; ++i)
    {
        writeValue(&val->u.array.values[i], ctx, writeBuf, writeFunc);
        if (i != val->u.array.len - 1)
            writeChar(',', ctx, writeBuf, writeFunc);
    }

    writeChar(']', ctx, writeBuf, writeFunc);
}

static void writeTrue(void *ctx, char *writeBuf, int (*writeFunc)(void *ctx, void *buf, int len))
{
    writeString("true", ctx, writeBuf, writeFunc);
}

static void writeFalse(void *ctx, char *writeBuf, int (*writeFunc)(void *ctx, void *buf, int len))
{
    writeString("false", ctx, writeBuf, writeFunc);
}

static void writeNull(void *ctx, char *writeBuf, int (*writeFunc)(void *ctx, void *buf, int len))
{
    writeString("null", ctx, writeBuf, writeFunc);
}

static void writeValue(
        const struct JsonVal *val,
        void *ctx, char *writeBuf,
        int (*writeFunc)(void *ctx, void *buf, int len))
{
    switch (val->type)
    {
        case jsonStringT:
            writeQuotedString(val->u.string, ctx, writeBuf, writeFunc);
            break;
        case jsonNumberT:
            writeNumber(val->u.number, ctx, writeBuf, writeFunc);
            break;
        case jsonObjectT:
            writeObject(val, ctx, writeBuf, writeFunc);
            break;
        case jsonArrayT:
            writeArray(val, ctx, writeBuf, writeFunc);
            break;
        case jsonTrueT:
            writeTrue(ctx, writeBuf, writeFunc);
            break;
        case jsonFalseT:
            writeFalse(ctx, writeBuf, writeFunc);
            break;
        case jsonNullT:
            writeNull(ctx, writeBuf, writeFunc);
            break;
    }
}

static void writeObject(
        const struct JsonVal *val,
        void *ctx, char *writeBuf,
        int (*writeFunc)(void *ctx, void *buf, int len))
{
    int i;

    writeChar('{', ctx, writeBuf, writeFunc);

    for (i = 0; i < val->u.object.len; ++i)
    {
        writeQuotedString(val->u.object.keys[i], ctx, writeBuf, writeFunc);
        writeChar(':', ctx, writeBuf, writeFunc);
        writeValue(&val->u.object.values[i], ctx, writeBuf, writeFunc);

        if (i != val->u.object.len - 1)
            writeChar(',', ctx, writeBuf, writeFunc);
    }

    writeChar('}', ctx, writeBuf, writeFunc);
}

struct CountingContext
{
    void *userCtx;
    int (*userWriteFunc)(void *ctx, void *buf, int len);
    int totalWrittenBytes;
};

static int countingWriteWrapper(void *ctx, void *buf, int len)
{
    struct CountingContext *countingCtx = (struct CountingContext *)ctx;
    countingCtx->totalWrittenBytes += len;

    return countingCtx->userWriteFunc(countingCtx->userCtx, buf, len);
}

int JsonVal_write(
        const struct JsonVal *val, void *ctx,
        int (*writeFunc)(void *ctx, void *buf, int len))
{
    char writeBuf[kWriteBufSize];
    struct CountingContext countingCtx = {ctx, writeFunc, 0};

    writeObject(val, &countingCtx, writeBuf, &countingWriteWrapper);
    writeBuf[0] = '\0';
    countingWriteWrapper(&countingCtx, writeBuf, 1);

    return countingCtx.totalWrittenBytes;
}

int JsonVal_writeString(const struct JsonVal *val, char *buf, int len)
{
    struct Buf ctx = {buf, len};
    return JsonVal_write(val, &ctx, &writeToBuf);
}

void JsonVal_destroy(struct JsonVal *val)
{
    int i;

    switch (val->type)
    {
        case jsonStringT:
            free(val->u.string);
            break;
        case jsonObjectT:
            for (i = 0; i < val->u.object.len; ++i)
            {
                JsonVal_destroy(&val->u.object.values[i]);
                free(val->u.object.keys[i]);
            }
            free(val->u.object.values);
            free(val->u.object.keys);
            break;
        case jsonArrayT:
            for (i = 0; i < val->u.array.len; ++i)
                JsonVal_destroy(&val->u.array.values[i]);
            free(val->u.array.values);
            break;
        default:
            break;
    }
}
