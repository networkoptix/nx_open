#pragma once

#include <functional>
#include <storage/third_party_storage.h>
#include <common.h>
#include <vfs.h>

struct FsStubNode;

class NX_TEST_STORAGE_PLUGIN_API TestStorage :
    public nx_spl::Storage,
    public PluginRefCounter<TestStorage>
{
public:
    TestStorage(const utils::VfsPair& vfsPair,
                const std::string& prefix,
                std::function<void()> onDestroyCb);
    virtual ~TestStorage();

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

    virtual int addRef() const override;
    virtual int releaseRef() const override;

private:
    virtual nx_spl::IODevice* createIODevice(const std::string& name,
        int category,
        int flags,
        int64_t size,
        int* ecode) const;

private:
    utils::VfsPair m_vfsPair;
    const std::string m_prefix;
    std::function<void()> m_onDestroyCb;
};
