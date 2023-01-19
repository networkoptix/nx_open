// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_info.h"

namespace nx::sdk {

const char* DeviceInfo::id() const
{
    return m_id.c_str();
}

const char* DeviceInfo::vendor() const
{
    return m_vendor.c_str();
}

const char* DeviceInfo::model() const
{
    return m_model.c_str();
}

const char* DeviceInfo::firmware() const
{
    return m_firmware.c_str();
}

const char* DeviceInfo::name() const
{
    return m_name.c_str();
}

const char* DeviceInfo::url() const
{
    return m_url.c_str();
}

const char* DeviceInfo::login() const
{
    return m_login.c_str();
}

const char* DeviceInfo::password() const
{
    return m_password.c_str();
}

const char* DeviceInfo::sharedId() const
{
    return m_sharedId.c_str();
}

const char* DeviceInfo::logicalId() const
{
    return m_logicalId.c_str();
}

int DeviceInfo::channelNumber() const
{
    return m_channelNumber;
}

void DeviceInfo::setId(std::string id)
{
    m_id = std::move(id);
}

void DeviceInfo::setVendor(std::string vendor)
{
    m_vendor = std::move(vendor);
}

void DeviceInfo::setModel(std::string model)
{
    m_model = std::move(model);
}

void DeviceInfo::setFirmware(std::string firmware)
{
    m_firmware = std::move(firmware);
}

void DeviceInfo::setName(std::string name)
{
    m_name = std::move(name);
}

void DeviceInfo::setUrl(std::string url)
{
    m_url = std::move(url);
}

void DeviceInfo::setLogin(std::string login)
{
    m_login = std::move(login);
}

void DeviceInfo::setPassword(std::string password)
{
    m_password = std::move(password);
}

void DeviceInfo::setSharedId(std::string sharedId)
{
    m_sharedId = std::move(sharedId);
}

void DeviceInfo::setLogicalId(std::string logicalId)
{
    m_logicalId = std::move(logicalId);
}

void DeviceInfo::setChannelNumber(int channelNumber)
{
    m_channelNumber = channelNumber;
}

} // namespace nx::sdk
