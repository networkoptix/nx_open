#pragma once

#include <third_party_storage.h>
#include <common.h>
#include <detail/fs_stub.h>


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
