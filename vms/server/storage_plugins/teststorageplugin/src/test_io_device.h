#pragma once

#include <string>
#include <storage/third_party_storage.h>
#include <common.h>
#include <detail/fs_stub.h>


enum class FileCategory
{
    media,
    db,
    infoTxt
};

class NX_TEST_STORAGE_PLUGIN_API TestIODevice :
    public nx_spl::IODevice,
    public PluginRefCounter<TestIODevice>
{
public:
    TestIODevice(const std::string& name, FileCategory category,
                 int mode, int64_t size = 0, FILE* f = nullptr);
    ~TestIODevice();
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

    virtual int addRef() const override;
    virtual int releaseRef() const override;

private:
    virtual uint32_t readImpl(void* dst, uint32_t size, int* ecode) const;
    virtual int seekImpl(uint64_t pos, int* ecode);

protected:
    const std::string m_sampleFileName;

private:
    FileCategory m_category;
    int m_mode;
    int64_t m_size;
    FILE* m_file;
    mutable int m_camInfoPos;
};
