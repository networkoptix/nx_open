#pragma once

#include <string>
#include <storage/third_party_storage.h>
#include <common.h>
#include <detail/fs_stub.h>

class TestFileInfoIterator :
    public nx_spl::FileInfoIterator,
    public PluginRefCounter<TestFileInfoIterator>
{
public:
    TestFileInfoIterator(FsStubNode* node, const std::string& prefix);
public:
    virtual nx_spl::FileInfo* STORAGE_METHOD_CALL next(int* ecode) const override;

public: // plugin interface implementation
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;

    virtual int addRef() const override;
    virtual int releaseRef() const override;

private:
    mutable nx_spl::FileInfo m_fInfo;
    mutable FsStubNode* m_cur;
    std::string m_prefix;
    mutable char m_urlBuf[4096];
};

