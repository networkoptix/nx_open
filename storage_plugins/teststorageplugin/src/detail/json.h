#pragma once

#include <stddef.h>

enum JsonType 
{
    jsonStringT,
    jsonNumberT,
    jsonObjectT,
    jsonArrayT,
    jsonTrueT,
    jsonFalseT,
    jsonNullT,
};

struct JsonVal 
{
    enum JsonType type;
    union
    {
        char *string;
        long double number;
        struct {
            char **keys;
            struct JsonVal *values;
            int len;
        } object;
        struct {
            struct JsonVal *values;
            int len;
        } array;
    } u;
};


struct JsonVal jsonParseString(const char *str, char *errorBuf, int errorBufSize);

/*creates empty (top-level) object*/
struct JsonVal jsonCreateObject();

/*check type functions*/
int JsonVal_isString(const struct JsonVal *val);
int JsonVal_isNumber(const struct JsonVal *val);
int JsonVal_isObject(const struct JsonVal *val);
int JsonVal_isArray(const struct JsonVal *val);
int JsonVal_isTrue(const struct JsonVal *val);
int JsonVal_isFalse(const struct JsonVal *val);
int JsonVal_isNull(const struct JsonVal *val);

/*add subvalues to existing values. return pointers to the newly created values.*/
struct JsonVal *JsonVal_objectAddString(struct JsonVal *val, const char *key, const char *value);
struct JsonVal *JsonVal_objectAddNumber(struct JsonVal *val, const char *key, long double number);
struct JsonVal *JsonVal_objectAddObject(struct JsonVal *val, const char *key);
struct JsonVal *JsonVal_objectAddArray(struct JsonVal *val, const char *key);
struct JsonVal *JsonVal_objectAddTrue(struct JsonVal *val, const char *key);
struct JsonVal *JsonVal_objectAddFalse(struct JsonVal *val, const char *key);
struct JsonVal *JsonVal_objectAddNull(struct JsonVal *val, const char *key);

struct JsonVal *JsonVal_arrayAddString(struct JsonVal *val, const char *value);
struct JsonVal *JsonVal_arrayAddNumber(struct JsonVal *val, long double number);
struct JsonVal *JsonVal_arrayAddObject(struct JsonVal *val);
struct JsonVal *JsonVal_arrayAddArray(struct JsonVal *val);
struct JsonVal *JsonVal_arrayAddTrue(struct JsonVal *val);
struct JsonVal *JsonVal_arrayAddFalse(struct JsonVal *val);
struct JsonVal *JsonVal_arrayAddNull(struct JsonVal *val);

/*access functions*/
int JsonVal_arrayLen(struct JsonVal *val);
struct JsonVal *JsonVal_arrayAt(struct JsonVal* val, int index);

struct JsonVal *JsonVal_getObjectValueByKey(const struct JsonVal *val, const char *key);

void JsonVal_forEachArrayElement(
        const struct JsonVal *val,
        void *ctx,
        void (*action)(void *, const struct JsonVal *));

void JsonVal_forEachObjectElement(
        const struct JsonVal *val,
        void *ctx,
        void (*action)(void *, const char *key, const struct JsonVal *));

/*write functions*/
int JsonVal_write(
        const struct JsonVal *val,
        /*User context. Will be passed to the writeFunc callback.*/
        void *ctx,
        /*user supplied write callback function.*/
        int (*writeFunc)(void *ctx, void *buf, int len));

/*returns total bytes required*/
int JsonVal_writeString(const struct JsonVal *val, char *buf, int len);

void JsonVal_destroy(struct JsonVal *val);
