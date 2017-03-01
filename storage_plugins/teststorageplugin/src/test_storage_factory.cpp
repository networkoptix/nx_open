#include <string>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include "test_storage_factory.h"
#include "log.h"
#include "vfs.h"
#include "url.h"

const char** STORAGE_METHOD_CALL TestStorageFactory::findAvailable() const
{
    return nullptr;
}

nx_spl::Storage* STORAGE_METHOD_CALL TestStorageFactory::createStorage(
    const char* url,
    int*        ecode
)
{
    if (m_storageUrls.find(url) != m_storageUrls.cend())
    {
        LOG("[TestStorage, TestStorageFactory::createStorage]: storage with this url '%s' already exists\n", url);
        if (ecode)
            *ecode = nx_spl::error::UrlNotExists;
        return nullptr;
    }

    utils::Url parsedUrl(url);
    if (!parsedUrl.valid())
    {
        LOG("[TestStorage, TestStorageFactory::createStorage]: url '%s' is invalid\n", url);
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

    // utils::VfsPair vfsPair;
    // if (!utils::buildVfsFromJson())
    // return new TestStorage();
    return nullptr;
}

const char* STORAGE_METHOD_CALL TestStorageFactory::storageType() const
{
    return "test";
}

const char* TestStorageFactory::lastErrorMessage(int ecode) const
{
    return errorCodeToString((ErrorCode)ecode);
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

unsigned int TestStorageFactory::addRef()
{
    return pAddRef();
}

unsigned int TestStorageFactory::releaseRef()
{
    return pReleaseRef();
}