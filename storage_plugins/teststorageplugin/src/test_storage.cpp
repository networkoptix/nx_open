#include <cstdlib>
#include <cstring>
#include <memory>
#include <test_storage.h>
#include <detail/fs_stub.h>
#include <test_file_info_iterator.h>
#include <url.h>
#include <test_io_device.h>
#include <log.h>


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

nx_spl::IODevice* STORAGE_METHOD_CALL TestStorage::open(const char* url, int flags, int* ecode) const 
{
    FileCategory category = FileCategory::media;
    if (strstr(url, ".nxdb") != nullptr)
        category = FileCategory::db;
    else if (strstr(url, "info.txt") != nullptr)
        category = FileCategory::infoTxt;

    auto filePath = urlToPath(url);
    FsStubNode* fileNode = FsStubNode_find(m_vfsPair.root, filePath.c_str());
    if (flags | nx_spl::io::WriteOnly && fileNode == nullptr)
    {
        fileNode = FsStubNode_add(m_vfsPair.root, filePath.c_str(), file, 660, 1);
        if (fileNode == nullptr)
        {
            LOG("[TestStorage, Open, IODevice] failed to add node with url %s\n", url);
            return nullptr;
        }
    }

    if (fileNode == nullptr)
        return nullptr;

    return createIODevice(fileNode->name, (int)category, flags, 1);
}

nx_spl::IODevice* TestStorage::createIODevice(
    const std::string& name, 
    int category, 
    int flags, 
    int size) const
{
    FILE* f = nullptr;
    if ((FileCategory)category == FileCategory::media)
    {
        f = fopen(m_vfsPair.sampleFilePath.c_str(), "rb");
        if (f == nullptr)
        {
            LOG("[TestStorage, Open, IODevice] failed to open sample file %s for read\n", 
                m_vfsPair.sampleFilePath.c_str());
            return nullptr;
        }
    }

    return new TestIODevice(name, (FileCategory)category, flags, size, f);
}

uint64_t STORAGE_METHOD_CALL TestStorage::getFreeSpace(int* ecode) const 
{
    return 5000LL * 1024 * 1024 * 1024;
}

uint64_t STORAGE_METHOD_CALL TestStorage::getTotalSpace(int* ecode) const 
{
    return 10000LL * 1024 * 1024 * 1024;
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