#include <string.h>
#include <stdio.h>
#include "test_io_device.h"

void* TestIODevice::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp(&interfaceID, &nx_spl::IID_IODevice, sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nx_spl::IODevice*>(this);
    }
    else if (memcmp(&interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface)) == 0) 
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

uint32_t TestIODevice::write(const void* /*src*/, const uint32_t size, int* ecode) 
{
    if (ecode)
        *ecode = error::NoError;
    return size;
}

uint32_t TestIODevice::read(void* dst, const uint32_t size, int* ecode) const
{
    if (!m_file)
    {
        if (ecode)
            *ecode = error::UrlNotExists;
        return 0;
    }
    return fread(dst, size, 1, m_file);
}

int TestIODevice::getMode() const 
{
    return m_mode;
}

uint32_t TestIODevice::size(int* ecode) const
{
    return 0;
}

int TestIODevice::seek(uint64_t pos, int* ecode)
{
    int result = fseek(m_file, pos, 0);
    if (result != 0)
    {
        if (ecode)
            *ecode = error::UnknownError;
        return 0;
    }

    return 1
}

TestIODevice::~TestIODevice()
{
    if (m_file)
        fclose(m_file);
}

TestIODevice::TestIODevice(FsStubNode* fileNode, FileCategory category, 
                           int mode, int64_t size, FILE* f) :
    m_fileNode(fileNode),
    m_category(category),
    m_mode(mode),
    m_size(size),
    m_file(file)
{
}