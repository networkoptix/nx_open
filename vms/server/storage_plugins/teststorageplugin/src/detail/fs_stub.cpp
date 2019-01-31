#include <string.h>
#include <stdlib.h>
#include "fs_stub.h"

static int createStringCopy(const char *source, int sourceLen, char **target)
{
    char *stringCopy;

    if (sourceLen == -1)
        sourceLen = strlen(source);

    stringCopy = (char *)malloc(sourceLen + 1);
    if (stringCopy == NULL)
        return -1;

    memcpy(stringCopy, source, sourceLen);
    stringCopy[sourceLen] = '\0';
    *target = stringCopy;

    return 0;
}

struct StringVector
{
    int len;
    char **data;
};

void StringVector_Init(struct StringVector *stringVector)
{
    memset(stringVector, 0, sizeof(*stringVector));
}

void StringVector_destroy(struct StringVector *stringVector)
{
    int i;

    for (i = 0; i < stringVector->len; ++i)
        free(stringVector->data[i]);

    free(stringVector->data);
    memset(stringVector, 0, sizeof(*stringVector));
}

char *StringVector_add(struct StringVector *stringVector, const char* source, int sourceLen)
{
    stringVector->data = (char **)realloc(stringVector->data, sizeof(*stringVector->data)*(stringVector->len + 1));
    if (stringVector->data == NULL)
        return NULL;

    if (createStringCopy(source, sourceLen, stringVector->data + stringVector->len) != 0)
        return NULL;

    stringVector->len++;
    return *(stringVector->data + stringVector->len - 1);
}

static void skipChar(const char **source, char c)
{
    while (**source == c)
        ++*source;
}

static struct StringVector split(const char *source, char separator)
{
    struct StringVector result;
    const char *start;

    StringVector_Init(&result);
    skipChar(&source, separator);

    start = source;
    while (*source)
    {
        if (*source == separator)
        {
            StringVector_add(&result, start, source - start);
            skipChar(&source, separator);
            start = source;
            if (!*source)
                break;
        }
        ++source;
    }

    if (source != start)
        StringVector_add(&result, start, source - start);

    return result;
}

static struct FsStubNode *createNode(
        struct FsStubNode *parent,
        const char *name,
        int type,
        int mask,
        int64_t size)
{
    struct FsStubNode *fsNode;
    struct FsStubNode *iteratorNode;
    struct FsStubNode *prevIteratorNode;

    fsNode = (struct FsStubNode *)malloc(sizeof(*fsNode));
    memset(fsNode, 0, sizeof(*fsNode));

    if (fsNode == NULL)
        return NULL;

    fsNode->mask = mask;
    fsNode->parent = parent;

    if (createStringCopy(name, -1, &fsNode->name) != 0)
        return NULL;

    if (parent != NULL)
    {
        prevIteratorNode = iteratorNode = parent->child;
        while (1)
        {
            if (iteratorNode == NULL)
                break;
            prevIteratorNode = iteratorNode;
            iteratorNode = iteratorNode->next;
        }

        fsNode->prev = prevIteratorNode;

        if (prevIteratorNode)
            prevIteratorNode->next = fsNode;
        else
            parent->child = fsNode;
    }
    else
        fsNode->prev = NULL;

    fsNode->size = size;
    fsNode->type = (enum FsStubEntryType)type;

    return fsNode;
}

struct FsStubNode *fsStubCreateTopLevel()
{
    return createNode(NULL, "/", dir, 0, 0);
}

struct FsStubNode *FsStubNode_add(
        struct FsStubNode *parent,
        const char *path,
        int type,
        int mask,
        int64_t size)
{
    struct FsStubNode *fsNode = NULL;
    struct FsStubNode *child;
    struct StringVector stringVector;
    int i;
    int childFound;

    stringVector = split(path, '/');

    for (i = 0; i < stringVector.len; ++i)
    {
        childFound = 0;
        for (child = parent->child; child; child = child->next)
        {
            if (strcmp(stringVector.data[i], child->name) == 0)
            {
                fsNode = child;
                childFound = 1;
                break;
            }
        }

        if (!childFound)
        {
            if (i == stringVector.len - 1)
                fsNode = createNode(parent, stringVector.data[i], type, mask, size);
            else
                fsNode = createNode(parent, stringVector.data[i], dir, 640, 0);
        }
        parent = fsNode;
    }

    StringVector_destroy(&stringVector);

    return fsNode;
}

struct FsStubNode *FsStubNode_find(struct FsStubNode *topLevelNode, const char *path)
{
    struct FsStubNode *fsNode = NULL;
    struct StringVector stringVector;
    int stringVectorIndex;
    int found;

    stringVector = split(path, '/');
    if (stringVector.len == 0)
        return NULL;

    stringVectorIndex = 0;

    while (stringVectorIndex < stringVector.len)
    {
        found = 0;
        for (fsNode = topLevelNode->child; fsNode; fsNode = fsNode->next)
        {
            if (strcmp(stringVector.data[stringVectorIndex], fsNode->name) == 0)
            {
                found = 1;
                topLevelNode = fsNode;
                break;
            }
        }

        if (!found)
        {
            StringVector_destroy(&stringVector);
            return NULL;
        }

        ++stringVectorIndex;
    }

    StringVector_destroy(&stringVector);
    return fsNode;
}

int FsStubNode_rename(struct FsStubNode *node, const char *newName)
{
    if (node->name)
        free(node->name);
    return createStringCopy(newName, -1, &node->name);
}

void FsStubNode_remove(struct FsStubNode *fsNode)
{
    struct FsStubNode *curNode, *tmp;

    curNode = fsNode->child;
    if (curNode)
        fsNode->child = NULL;

    while (curNode)
    {
        tmp = curNode;
        curNode = curNode->next;
        FsStubNode_remove(tmp);
    }

    if (fsNode->prev)
        fsNode->prev->next = fsNode->next;
    else if (fsNode->parent)
        fsNode->parent->child = fsNode->next;

    if (fsNode->next)
        fsNode->next->prev = NULL;

    free(fsNode->name);
    free(fsNode);
}

void FsStubNode_forEach(
    struct FsStubNode *root, 
    void *ctx, 
    void (*action)(void *ctx, struct FsStubNode *node))
{
    struct FsStubNode *curNode;

    curNode = root->child;
    for (; curNode; curNode = curNode->next)
        FsStubNode_forEach(curNode, ctx, action);

    action(ctx, root);
}

int FsStubNode_fullPath(struct FsStubNode *fsNode, char *buf, int size)
{
    int resultSize = 1;
    struct FsStubNode *curNode = fsNode;
    int writePos, nameLen;

    for (; curNode; curNode = curNode->parent)
        if (curNode->parent)
            resultSize += strlen(curNode->name) + 1;

    if (resultSize > size)
        return resultSize;

    curNode = fsNode;
    writePos = resultSize - 1;
    for (; curNode; curNode = curNode->parent)
    { 
        nameLen = strlen(curNode->name);
        if (curNode->parent)
        {
            memcpy(buf + writePos - nameLen, curNode->name, nameLen);
            writePos -= nameLen + 1;
            buf[writePos] = '/';
        }
    }
    buf[resultSize - 1] = '\0';

    return resultSize;
}
