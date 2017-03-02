#include <cstdlib>
#include <cstring>
#include <memory>
#include <test_storage.h>
#include <detail/fs_stub.h>
#include <test_file_info_iterator.h>
#include <url.h>

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

*/

using namespace nx_spl;

namespace {

std::string urlToPath(const char* url)
{
    return utils::Url(url).hostPath();
}

std::string lastPathSegment(const std::string& path)
{
    if (path.empty())
        return path;

    auto pathCopy = path;
    if (path[path.size() - 1] == '/')
        pathCopy.assign(path.cbegin(), path.cend()-1);

    auto lastSeparatorPos = pathCopy.rfind('/');
    if (lastSeparatorPos == std::string::npos)
        return path;

    return pathCopy.substr(lastSeparatorPos + 1);
}

void removeNode(struct FsStubNode* root, enum FsStubEntryType type, const char* url, int* ecode)
{
    struct FsStubNode* nodeToRemove = FsStubNode_find(root, url);
    if (nodeToRemove == nullptr || nodeToRemove->type != type)
    {
        if (ecode)
            *ecode = error::UrlNotExists;
        return;
    } 

    FsStubNode_remove(nodeToRemove);
    if (ecode)
        *ecode = error::NoError;
}

bool nodeExists(struct FsStubNode* root, const char* url, int* ecode)
{
    if (FsStubNode_find(root, url) == nullptr)
    {
        if (ecode)
            *ecode = error::UrlNotExists;
        return false;
    }

    if (ecode)
        *ecode = error::NoError;

    return true;
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
    return 500 * 1024 * 1024 * 1024LL;
}

uint64_t STORAGE_METHOD_CALL TestStorage::getTotalSpace(int* ecode) const 
{
    return 1000 * 1024 * 1024 * 1024LL;
}

int STORAGE_METHOD_CALL TestStorage::getCapabilities() const 
{
    return cap::ListFile | cap::RemoveFile | cap::ReadFile | cap::WriteFile | cap::DBReady;
}

void STORAGE_METHOD_CALL TestStorage::removeFile(
    const char* url,
    int*        ecode
) 
{
    removeNode(m_vfsPair.root, file, urlToPath(url).c_str(), ecode);
}

void STORAGE_METHOD_CALL TestStorage::removeDir(
    const char* url,
    int*        ecode
)
{
    removeNode(m_vfsPair.root, dir, urlToPath(url).c_str(), ecode);
} 

void STORAGE_METHOD_CALL TestStorage::renameFile(
    const char*     oldUrl,
    const char*     newUrl,
    int*            ecode
) 
{
    if (strcmp(oldUrl, newUrl) == 0)
    {
        if (ecode)
            *ecode = nx_spl::error::NoError;
        return;
    }

    struct FsStubNode* nodeToRename = FsStubNode_find(m_vfsPair.root, urlToPath(oldUrl).c_str());
    if (nodeToRename == nullptr || nodeToRename->type != file)
    {
        if (ecode)
            *ecode = error::UrlNotExists;
        return;
    }

    auto resultNode = FsStubNode_add(
        m_vfsPair.root, 
        urlToPath(newUrl).c_str(), 
        file,
        660,
        nodeToRename->size);

    if (resultNode == nullptr)
    {
        if (ecode)
            *ecode = error::UnknownError;
        return;
    }

    FsStubNode_remove(nodeToRename);
    if (ecode)
        *ecode = error::NoError;
}

nx_spl::FileInfoIterator* STORAGE_METHOD_CALL TestStorage::getFileIterator(
    const char*     dirUrl,
    int*            ecode
) const 
{
    struct FsStubNode* fsNode = FsStubNode_find(m_vfsPair.root, urlToPath(dirUrl).c_str());
    if (fsNode == nullptr)
    {
        if (ecode)
            *ecode = error::UrlNotExists;
        return nullptr;
    }

    return new TestFileInfoIterator(fsNode);
}

int STORAGE_METHOD_CALL TestStorage::fileExists(
    const char*     url,
    int*            ecode
) const 
{
    return nodeExists(m_vfsPair.root, urlToPath(url).c_str(), ecode);
}

int STORAGE_METHOD_CALL TestStorage::dirExists(
    const char*     url,
    int*            ecode
) const 
{
    return nodeExists(m_vfsPair.root, urlToPath(url).c_str(), ecode);
}

uint64_t STORAGE_METHOD_CALL TestStorage::fileSize(
    const char*     url,
    int*            ecode
) const 
{
    struct FsStubNode* fsNode = FsStubNode_find(m_vfsPair.root, urlToPath(url).c_str());
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