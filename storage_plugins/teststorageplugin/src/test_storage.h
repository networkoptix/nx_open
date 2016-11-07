#pragma once

#include <stdio.h>

#include "plugins/storage/third_party/third_party_storage.h"

namespace {
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
}

class TestIODevice 
    : public nx_spl::IODevice,
      private PluginRefCounter<TestIODevice>
{
    ~TestIODevice();
public:
    TestIODevice(const char *path, ns_spl::io::mode_t mode);
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
    virtual FileInfo* STORAGE_METHOD_CALL next(int* ecode) override;
private:
    nx_spl::FileInfo m_fInfo;
};

