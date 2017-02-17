#pragma once

#include <stddef.h>
#include <stdint.h>

enum FsStubEntryType
{
    dir = 1,
    file = 2
};

struct FsStubNode
{
    char *name;
    int mask;
    enum FsStubEntryType type;
    int64_t size;
    struct FsStubNode *next;
    struct FsStubNode *prev;
    struct FsStubNode *parent;
    struct FsStubNode *child;
};

struct FsStubNode *fsStubCreateTopLevel();
struct FsStubNode *FsStubNode_add(
        struct FsStubNode *parent,
        const char *path,
        int type,
        int mask,
        int64_t size);

struct FsStubNode *FsStubNode_find(struct FsStubNode *topLevelNode, const char *path);
int FsStubNode_rename(struct FsStubNode *fsNode, const char *newName);
void FsStubNode_remove(struct FsStubNode *fsNode);

