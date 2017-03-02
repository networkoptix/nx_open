#pragma once

#include "third_party_storage.h"
#include "common.h"
#include "vfs.h"

/*

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

*/

struct FsStubNode;

class TestStorage : 
    public nx_spl::Storage, 
    public PluginRefCounter<TestStorage>
{
public:
    TestStorage(const utils::VfsPair& vfsPair);

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

private:
    utils::VfsPair m_vfsPair;
};
