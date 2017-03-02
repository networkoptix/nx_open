#include <cstdlib>
#include <cstring>
#include <memory>
#include "test_storage.h"
#include <detail/fs_stub.h>

/*
// IODevice
TestIODevice::TestIODevice(const char *path, nx_spl::io::mode_t mode)
: m_mode(mode)
{
m_file = fopen(path, "rb");
}

TestIODevice::~TestIODevice()
{
if (m_file)
    fclose(m_file);
}

uint32_t STORAGE_METHOD_CALL TestIODevice::write(
const void*     src,
const uint32_t  size,
int*            ecode)
{
return 0;
}

uint32_t STORAGE_METHOD_CALL TestIODevice::read(
void*           dst,
const uint32_t  size,
int*            ecode) const
{
return 0;
}


int STORAGE_METHOD_CALL TestIODevice::getMode() const
{
return m_mode;
}

uint32_t STORAGE_METHOD_CALL TestIODevice::size(int* ecode) const
{
return 0;
}

int STORAGE_METHOD_CALL TestIODevice::seek(
uint64_t    pos,
int*        ecode)
{
return 0;
}

void* TestIODevice::queryInterface(const nxpl::NX_GUID& interfaceID)
{
if (std::memcmp(&interfaceID,
                &nx_spl::IID_IODevice,
                sizeof(nxpl::NX_GUID)) == 0)
{
    addRef();
    return static_cast<nx_spl::IODevice*>(this);
}
else if (std::memcmp(&interfaceID,
                         &nxpl::IID_PluginInterface, 
                         sizeof(nxpl::IID_PluginInterface)) == 0) 
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

unsigned int TestIODevice::addRef()
{
    return pAddRef();
}

unsigned int TestIODevice::releaseRef()
{
    return pReleaseRef();
}

// FileInfoIterator
nx_spl::FileInfo* STORAGE_METHOD_CALL TestFileInfoIterator::next(int* ecode) const
{
    return nullptr;
}

*/

using namespace nx_spl;

namespace {
void removeNode(struct FsStubNode* root, const char* url, int* ecode)
{
    struct FsStubNode* nodeToRemove = FsStubNode_find(root, url);
    if (nodeToRemove == nullptr)
    {
        if (ecode)
            *ecode = error::UrlNotExists;
        return;
    } 

    FsStubNode_remove(nodeToRemove);
    if (ecode)
        *ecode = error::NoError;
}

int nodeExists(struct FsStubNode* root, const char* url, int* ecode)
{
    if (FsStubNode_find(root, url) == nullptr)
    {
        if (ecode)
            *ecode = error::UrlNotExists;
        return 0;
    }

    if (ecode)
        *ecode = error::NoError;

    return 1;
}

}

TestStorage::TestStorage(const utils::VfsPair& vfsPair) : m_vfsPair(vfsPair) {}

int STORAGE_METHOD_CALL TestStorage::isAvailable() const
{
    return true;
}

nx_spl::IODevice* STORAGE_METHOD_CALL TestStorage::open(
    const char*     url,
    int             flags,
    int*            ecode
) const 
{
    return nullptr;
}

uint64_t STORAGE_METHOD_CALL TestStorage::getFreeSpace(int* ecode) const 
{
    return 1000 * 1024 * 1024 * 1024ll;
}

uint64_t STORAGE_METHOD_CALL TestStorage::getTotalSpace(int* ecode) const 
{
    return 500 * 1024 * 1024 * 1024ll;
}

int STORAGE_METHOD_CALL TestStorage::getCapabilities() const 
{
    return cap::ListFile | cap::RemoveFile | cap::ReadFile | cap::WriteFile;
}

void STORAGE_METHOD_CALL TestStorage::removeFile(
    const char* url,
    int*        ecode
) 
{
    removeNode(m_vfsPair.root, url, ecode);
}

void STORAGE_METHOD_CALL TestStorage::removeDir(
    const char* url,
    int*        ecode
)
{
    removeNode(m_vfsPair.root, url, ecode);
} 

void STORAGE_METHOD_CALL TestStorage::renameFile(
    const char*     oldUrl,
    const char*     newUrl,
    int*            ecode
) 
{
    struct FsStubNode* nodeToRename = FsStubNode_find(m_vfsPair.root, oldUrl);
    if (nodeToRename == nullptr)
    {
        if (ecode)
            *ecode = error::UrlNotExists;
        return;
    }

    int result = FsStubNode_rename(nodeToRename, newUrl);
    if (result != 0)
    {
        if (ecode)
            *ecode = error::UnknownError;
        return;
    }

    if (ecode)
        *ecode = error::NoError;
}

nx_spl::FileInfoIterator* STORAGE_METHOD_CALL TestStorage::getFileIterator(
    const char*     dirUrl,
    int*            ecode
) const 
{
    return nullptr;
}

int STORAGE_METHOD_CALL TestStorage::fileExists(
    const char*     url,
    int*            ecode
) const 
{
    return nodeExists(m_vfsPair.root, url, ecode);
}

int STORAGE_METHOD_CALL TestStorage::dirExists(
    const char*     url,
    int*            ecode
) const 
{
    return nodeExists(m_vfsPair.root, url, ecode);
}

uint64_t STORAGE_METHOD_CALL TestStorage::fileSize(
    const char*     url,
    int*            ecode
) const 
{
    struct FsStubNode* fsNode = FsStubNode_find(m_vfsPair.root, url);
    if (fsNode == nullptr)
    {
        if (ecode)
            *ecode = error::UrlNotExists;
        return unknown_size;
    }

    return fsNode->size;
}

void* TestStorage::queryInterface(const nxpl::NX_GUID& interfaceID) 
{
    if (std::memcmp(&interfaceID,
                    &nx_spl::IID_Storage,
                    sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nx_spl::Storage*>(this);
    }
    else if (std::memcmp(&interfaceID,
                         &nxpl::IID_PluginInterface,
                         sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

unsigned int TestStorage::addRef() 
{
    return pAddRef();
}

unsigned int TestStorage::releaseRef() 
{
    return pReleaseRef();
}