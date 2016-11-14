#pragma once

enum VfsType
{
    file,
    dir
};

struct VfsNode
{
    enum VfsType type;
    const char *path;
    struct VfsNode **subNodes;
};

struct VfsNode *VfsNode_init(struct VfsNode *node);
struct VfsNode *VfsNode_addNode(struct VfsNode *node, enum VfsType type, const char *path);

void VfsNode_destroy(struct VfsNode *node);

struct Vfs
{
    struct VfsNode *head;
};
