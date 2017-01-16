#pragma once

#include <atomic>
#include <cstdio>
#include "third_party_storage.h"

#define ERROR_CODE_LIST(APPLY) \
    APPLY(ok)

#define ERROR_CODE_APPLY_ENUM(value) value,

enum ErrorCode
{
    ERROR_CODE_LIST(ERROR_CODE_APPLY_ENUM)
};


template <typename P>
class PluginRefCounter
{
public:
    PluginRefCounter()
        : m_count(1)
    {}

    int pAddRef() { return ++m_count; }

    int pReleaseRef()
    {
        int new_count = --m_count;
        if (new_count <= 0) 
            delete static_cast<P*>(this);

        return new_count;
    }
private:
    std::atomic<int> m_count;
}; // class PluginRefCounter

class TestIODevice 
    : public nx_spl::IODevice,
      private PluginRefCounter<TestIODevice>
{
    friend class PluginRefCounter<TestIODevice>;
    ~TestIODevice();
public:
    TestIODevice(const char *path, nx_spl::io::mode_t mode);
public:
    virtual uint32_t STORAGE_METHOD_CALL write(
        const void*     src,
        const uint32_t  size,
        int*            ecode
    ) override;

    virtual uint32_t STORAGE_METHOD_CALL read(
        void*           dst,
        const uint32_t  size,
        int*            ecode
    ) const override;

    virtual int STORAGE_METHOD_CALL getMode() const override;

    virtual uint32_t STORAGE_METHOD_CALL size(int* ecode) const override;

    virtual int STORAGE_METHOD_CALL seek(
        uint64_t    pos, 
        int*        ecode
    ) override;

public: // plugin interface implementation
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;

    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;
private:
    FILE* m_file;
    nx_spl::io::mode_t m_mode;
    uint32_t m_size;
};

class TestFileInfoIterator
    : public nx_spl::FileInfoIterator,
      private PluginRefCounter<TestFileInfoIterator> 
{
public:
    TestFileInfoIterator(const char *path);
public:
    virtual nx_spl::FileInfo* STORAGE_METHOD_CALL next(int* ecode) const override;
private:
    nx_spl::FileInfo m_fInfo;
};

//!Storage abstraction
class TestStorage : public nx_spl::Storage
{
public:
    virtual int STORAGE_METHOD_CALL isAvailable() const override;

    virtual nx_spl::IODevice* STORAGE_METHOD_CALL open(
        const char*     url,
        int             flags,
        int*            ecode
    ) const override;

    virtual uint64_t STORAGE_METHOD_CALL getFreeSpace(int* ecode) const override;

    virtual uint64_t STORAGE_METHOD_CALL getTotalSpace(int* ecode) const override;

    virtual int STORAGE_METHOD_CALL getCapabilities() const override;

    virtual void STORAGE_METHOD_CALL removeFile(
        const char* url,
        int*        ecode
    ) override;

    virtual void STORAGE_METHOD_CALL removeDir(
        const char* url,
        int*        ecode
    ) override;

    virtual void STORAGE_METHOD_CALL renameFile(
        const char*     oldUrl,
        const char*     newUrl,
        int*            ecode
    ) override;

    virtual nx_spl::FileInfoIterator* STORAGE_METHOD_CALL getFileIterator(
        const char*     dirUrl,
        int*            ecode
    ) const override;

    virtual int STORAGE_METHOD_CALL fileExists(
        const char*     url,
        int*            ecode
    ) const override;

    virtual int STORAGE_METHOD_CALL dirExists(
        const char*     url,
        int*            ecode
    ) const override;

    virtual uint64_t STORAGE_METHOD_CALL fileSize(
        const char*     url,
        int*            ecode
    ) const override;

public: // plugin interface implementation
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;

    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

public:
    ErrorCode parseUrl(const char* url);

};

// {2E2C7A3D-256D-4018-B40E-512D72510BEC}
static const nxpl::NX_GUID IID_StorageFactory =
{ { 0x2e, 0x2c, 0x7a, 0x3d, 0x25, 0x6d, 0x40, 0x18, 0xb4, 0xe, 0x51, 0x2d, 0x72, 0x51, 0xb, 0xec } };

class TestStorageFactory
    : public nx_spl::StorageFactory,
      public PluginRefCounter<TestStorageFactory>
{
public:
    virtual const char** STORAGE_METHOD_CALL findAvailable() const override;

    virtual nx_spl::Storage* STORAGE_METHOD_CALL createStorage(
        const char* url,
        int*        ecode
    ) override;

    virtual const char* STORAGE_METHOD_CALL storageType() const override;

    virtual const char* lastErrorMessage(int ecode) const override;

public: // plugin interface implementation
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;

    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;
};

