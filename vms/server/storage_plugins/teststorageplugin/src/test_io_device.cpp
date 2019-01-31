#include <algorithm>
#include <string.h>
#include <stdio.h>
#include <test_io_device.h>
#include <log.h>

namespace {

const char* const kCamInfo = 
    "\"cameraName\"=\"TestStorageCamera\"\n"
    "\"cameraModel\"=\"TestStorageCamera\"\n"
    "\"groupId\"=\"\"\n"
    "\"groupName\"=\"\"\n";

auto setEcode = [](int* ecode, nx_spl::error::code_t codeToSet)
{
    if (ecode)
        *ecode = codeToSet;
};

}

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
    if (!(m_mode & nx_spl::io::WriteOnly))
    {
        if (ecode)
            *ecode = nx_spl::error::WriteNotSupported;
        return 0;
    }

    if (ecode)
        *ecode = nx_spl::error::NoError;
    return size;
}

uint32_t TestIODevice::read(void* dst, const uint32_t size, int* ecode) const
{
    switch (m_category)
    {
    case FileCategory::db:
        setEcode(ecode, nx_spl::error::UnknownError);
        return 0;
    case FileCategory::infoTxt:
    {
        int bytesToRead = std::min(size, (uint32_t)strlen(kCamInfo) - m_camInfoPos);
        memcpy(dst, kCamInfo + m_camInfoPos, bytesToRead);
        m_camInfoPos += bytesToRead;
        setEcode(ecode, nx_spl::error::NoError);
        return bytesToRead;
    }
    case FileCategory::media:
        return readImpl(dst, size, ecode);
    }

    return 0;
}

uint32_t TestIODevice::readImpl(void* dst, uint32_t size, int* ecode) const
{
    if (!m_file)
    {
        setEcode(ecode, nx_spl::error::UrlNotExists);
        return 0;
    }
    setEcode(ecode, nx_spl::error::NoError);
    return fread(dst, 1, size, m_file);
}

int TestIODevice::getMode() const 
{
    return m_mode;
}

uint32_t TestIODevice::size(int* ecode) const
{
    if (ecode)
        *ecode = nx_spl::error::NoError;
    return m_size;
}

int TestIODevice::seek(uint64_t pos, int* ecode)
{
    switch (m_category)
    {
    case FileCategory::db:
        break;
    case FileCategory::infoTxt:
        if (pos < 0ULL || pos >= strlen(kCamInfo))
        {
            setEcode(ecode, nx_spl::error::UnknownError);
            return 0;
        }
        m_camInfoPos = pos;
        break;
    case FileCategory::media:
        if (seekImpl(pos, ecode) == 0)
            return 0;
        break;
    }

    setEcode(ecode, nx_spl::error::NoError);
    return 1;
}

int TestIODevice::seekImpl(uint64_t pos, int* ecode)
{
    if (m_file == nullptr)
    {
        setEcode(ecode, nx_spl::error::UnknownError);
        return 0;
    }

    int result = fseek(m_file, pos, 0);
    if (result != 0)
    {
        setEcode(ecode, nx_spl::error::UnknownError);
        return 0;
    }

    setEcode(ecode, nx_spl::error::NoError);
    return 1;
}

TestIODevice::~TestIODevice()
{
    if (m_file)
        fclose(m_file);
}

TestIODevice::TestIODevice(const std::string& name, FileCategory category, int mode, 
                           int64_t size, FILE* f) :
    m_sampleFileName(name),
    m_category(category),
    m_mode(mode),
    m_size(size),
    m_file(f),
    m_camInfoPos(0)
{
    switch (category)
    {
    case FileCategory::db:
        m_size = 0;
        break;
    case FileCategory::media:
        break;
    case FileCategory::infoTxt:
        m_size = strlen(kCamInfo);
        break;
    }
}
