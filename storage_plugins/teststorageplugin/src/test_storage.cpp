#include <cstdlib>
#include <cstring>
#include "test_storage.h"

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
    return NULL;
}
