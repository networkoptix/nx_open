#include <string>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <iterator>
#include "test_storage_factory.h"
#include "test_storage.h"
#include "log.h"
#include "vfs.h"

namespace {

#if defined (_WIN32)
#   include <windows.h>
#elif defined (__linux__)
#   include <unistd.h>
#   include <sys/types.h>
#endif

std::string getAppDir()
{
    std::string result;
    char buf[512];

#if defined (_WIN32)

	TCHAR tBuf[512];
    int bytes = GetModuleFileName(NULL, tBuf, 512);
    if (bytes == 0)
        return result;
    char separator = '\\';
	WideCharToMultiByte(CP_UTF8, 0, tBuf, 512, buf, 512, 0, 0);

#elif defined (__linux__)

    char tmp[64];
    sprintf(tmp, "/proc/%d/exe", getpid());
    int bytes = readlink(tmp, buf, sizeof buf);
    if (bytes <= 0)
        return result;
    char separator = '/';

#endif

    result = buf;
    auto lastSeparatorPos = result.rfind(separator);
    if (lastSeparatorPos == std::string::npos)
        return result;

    return result.substr(0, lastSeparatorPos);
}

}

const char** STORAGE_METHOD_CALL TestStorageFactory::findAvailable() const
{
    return nullptr;
}

bool TestStorageFactory::readConfig(const std::string& path, std::string* outContent)
{
    std::ifstream configFs(path);
    if (!configFs)
        return false;

    std::copy((std::istreambuf_iterator<char>(configFs)), (std::istreambuf_iterator<char>()), std::back_inserter(*outContent));
    return true;
}

nx_spl::Storage* STORAGE_METHOD_CALL TestStorageFactory::createStorage(
    const char* url,
    int*        ecode
)
{
    /* parse url */
    utils::Url parsedUrl(url);
    if (!parsedUrl.valid())
    {
        LOG("[TestStorage, TestStorageFactory::createStorage]: url '%s' is invalid\n", url);
        if (ecode)
            *ecode = nx_spl::error::UrlNotExists;
        return nullptr;
    }

    if (parsedUrl.scheme() != "test")
    {
        LOG("[TestStorage, TestStorageFactory::createStorage]: url '%s' is invalid (wrong scheme)\n", url);
        if (ecode)
            *ecode = nx_spl::error::UrlNotExists;
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_storageHostsMutex);
    /* check if already exists */
    if (m_storageHosts.find(parsedUrl.host()) != m_storageHosts.cend())
    {
        LOG("[TestStorage, TestStorageFactory::createStorage]: storage with this host '%s' already exists\n", parsedUrl.host().c_str());
        if (ecode)
            *ecode = nx_spl::error::UrlNotExists;
        return nullptr;
    }

    std::string configName;
    const std::string kConfigParamName = "config";
    utils::ParamsMap params = parsedUrl.params();

    if (params.find(kConfigParamName) != params.cend())
        configName = params[kConfigParamName];
    else
        configName = parsedUrl.host() + ".cfg";

    /* read config file */
    auto configFileFullPath = utils::fsJoin(getAppDir(), configName);
    std::string configContent;
    if (!readConfig(configFileFullPath, &configContent))
    {
        LOG("[TestStorage, TestStorageFactory::createStorage]: couldn't read config file '%s'\n", configFileFullPath.c_str());
        if (ecode)
            *ecode = nx_spl::error::UrlNotExists;
        return nullptr;
    }

    /* parse config json */
    utils::VfsPair vfsPair;
    if (!utils::buildVfsFromJson(configContent.c_str(),
        parsedUrl.path().empty() ? "/" : parsedUrl.path().c_str(),
        &vfsPair))
    {
        if (vfsPair.root)
            FsStubNode_remove(vfsPair.root);
        if (ecode)
            *ecode = nx_spl::error::UrlNotExists;

        return nullptr;
    }

    m_storageHosts.insert(parsedUrl.host());
    if (ecode)
        *ecode = nx_spl::error::NoError;

    return createStorageImpl(vfsPair, parsedUrl);
}

nx_spl::Storage* TestStorageFactory::createStorageImpl(
    const utils::VfsPair& vfsPair,
    const utils::Url& url)
{
    return new TestStorage(vfsPair, url.scheme() + "://" + url.host(),
        [this, url]()
        {
            std::lock_guard<std::mutex> lock(m_storageHostsMutex);
            m_storageHosts.erase(url.host());
        });
}

const char* STORAGE_METHOD_CALL TestStorageFactory::storageType() const
{
    return "test";
}

const char* TestStorageFactory::lastErrorMessage(int ecode) const
{
    using namespace nx_spl;
    switch ((error::code_t)ecode)
    {
    case error::NoError: return "No error";
    case error::StorageUnavailable: return "Storage is not available";
    case error::NotEnoughSpace: return "Not enough space";
    case error::SpaceInfoNotAvailable: return "Space info not available";
    case error::EndOfFile: return "End of file";
    case error::ReadNotSupported: return "Read not supported";
    case error::WriteNotSupported: return "Write not supported";
    case error::UrlNotExists: return "Url doesn't exist";
    case error::UnknownError: return "Unknown error";
    }

    return nullptr;
}

void* TestStorageFactory::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (std::memcmp(&interfaceID,
                    &nx_spl::IID_StorageFactory,
                    sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nx_spl::StorageFactory*>(this);
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

int TestStorageFactory::addRef() const
{
    return pAddRef();
}

int TestStorageFactory::releaseRef() const
{
    return pReleaseRef();
}

extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
    nxpl::PluginInterface* createNXPluginInstance()
    {
        return new TestStorageFactory();
    }
}
