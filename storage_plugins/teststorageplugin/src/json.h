#include <stdint.h>

enum JsonType 
{
    jsonStringT = 1,
    jsonNumberT = 2,
    jsonObjectT = 3,
    jsonArrayT = 4,
    jsonTrueT = 5,
    jsonFalseT = 6,
    jsonNullT = 7,
};

struct JsonVal 
{
    JsonType type;
    union
    {
        char *string;
        long double number;
        struct {
            const char **keys;
            JsonVal *values;
            size_t len;
        } object;
        struct {
            JsonVal *values;
            size_t len;
        } array;
    } u;
};

struct JsonVal jsonParseString(const char *str, char **errorBuf, int errorBufSize); 

void JsonVal_destroy(struct JsonVal *val);

int JsonVal_isString(const struct JsonVal *val); 
int JsonVal_isNumber(const struct JsonVal *val); 
int JsonVal_isObject(const struct JsonVal *val); 
int JsonVal_isArray(const struct JsonVal *val); 
int JsonVal_isTrue((const struct JsonVal *val); 
int JsonVal_isFalse(const struct JsonVal *val); 
int JsonVal_isNull(const struct JsonVal *val); 
