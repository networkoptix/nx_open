#include <cstdlib>
#include <cstring>
#include <memory>
#include "test_storage.h"

const char* errorCodeToString(ErrorCode ecode)
{
#define ERROR_CODE_APPLY_TO_STRING(value) case value: return #value;
    switch (ecode)
    {
    ERROR_CODE_LIST(ERROR_CODE_APPLY_TO_STRING)
    }
    return nullptr;
}

#define _TEST_STORAGE_DEBUG

#if defined (_TEST_STORAGE_DEBUG)
#	define LOG(fmtString, ...) do { \
        char buf[1024]; \
        sprintf(buf, "%s ", __FUNCTION__); \
        sprintf(buf + strlen(buf), fmtString, __VA_ARGS__); \
        fprintf(stderr, "%s\n", buf); \
        fflush(stderr); \
    } while (0)
#else
#	define LOG(...)
#endif

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

// StorageFactory
const char** STORAGE_METHOD_CALL TestStorageFactory::findAvailable() const
{
    return nullptr;
}

nx_spl::Storage* STORAGE_METHOD_CALL TestStorageFactory::createStorage(
    const char* url,
    int*        ecode
)
{
    auto testStorage = std::unique_ptr<TestStorage>(new TestStorage);
    auto errorCode = testStorage->parseUrl(url);
    if (errorCode != ok)
    {
        LOG("invalid url %s. error: %s", url, errorCodeToString(errorCode));
        if (ecode)
            *ecode = errorCode;
        return nullptr;
    }
    return testStorage.release();
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
