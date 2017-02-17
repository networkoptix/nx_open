#include <string>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include "test_storage_factory.h"

const char** STORAGE_METHOD_CALL TestStorageFactory::findAvailable() const
{
    return nullptr;
}

nx_spl::Storage* STORAGE_METHOD_CALL TestStorageFactory::createStorage(
    const char* url,
    int*        ecode
)
{

    // auto testStorage = std::unique_ptr<TestStorage>(new TestStorage);
    // auto errorCode = testStorage->parseUrl(url);
    // if (errorCode != ok)
    // {
    //     LOG("invalid url %s. error: %s", url, errorCodeToString(errorCode));
    //     if (ecode)
    //         *ecode = errorCode;
    //     return nullptr;
    // }
    // return testStorage.release();
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