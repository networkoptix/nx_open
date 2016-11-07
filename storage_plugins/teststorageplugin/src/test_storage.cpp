#include "test_storage.h"

#define FILL_ECODE(ec) \
    do { \
        if (ecode) \
            *ecode = ec; \
    } while (0)

#define IF_FILE(yesAction, noAction, noError) \
    do { \
        if (m_file) \
        { \
            FILL_ECODE(error::NoError); \
            yesAction; \
        } \
 \
        FILL_ECODE(noError); \
        noAction; \
    } while (0)

// IODevice
TestIODevice::TestIODevice(const char *path, io::mode_t mode)
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
    FILL_ECODE(error::NoError);
    return size;
}

uint32_t STORAGE_METHOD_CALL TestIODevice::read(
    void*           dst,
    const uint32_t  size,
    int*            ecode) const
{
    IF_FILE(
        return (unit32_t)fread(dst, 1, size, m_file),
        return 0,
        error::ReadNotSupported);
}


int STORAGE_METHOD_CALL TestIODevice::getMode() const
{
    return m_mode;
}

uint32_t STORAGE_METHOD_CALL TestIODevice::size(int* ecode) const
{
    IF_FILE(
        return m_size,
        return 0,
        error::SpaceInfoNotAvailable);
}

int STORAGE_METHOD_CALL TestIODevice::seek(
    uint64_t    pos, 
    int*        ecode) 
{
    IF_FILE(
        return fseek(m_file, pos, SEEK_SET),
        return -1,
        error::ReadNotSupported);
}

void* TestIODevice::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (std::memcmp(&interfaceID, 
                    &IID_FileInfoIterator, 
                    sizeof(nxpl::NX_GUID)) == 0) 
    {
        addRef();
        return static_cast<nx_spl::FileInfoIterator*>(this);
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
FileInfo* STORAGE_METHOD_CALL TestFileInfoIterator next(int* ecode) const
{
}
