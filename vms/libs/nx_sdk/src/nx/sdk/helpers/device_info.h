// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_device_info.h>

namespace nx::sdk {

class DeviceInfo: public nx::sdk::RefCountable<IDeviceInfo>
{
public:
    virtual const char* id() const override;
    virtual const char* vendor() const override;
    virtual const char* model() const override;
    virtual const char* firmware() const override;
    virtual const char* name() const override;
    virtual const char* url() const override;
    virtual const char* login() const override;
    virtual const char* password() const override;
    virtual const char* sharedId() const override;
    virtual const char* logicalId() const override;
    virtual int channelNumber() const override;

    void setId(std::string id);
    void setVendor(std::string vendor);
    void setModel(std::string model);
    void setFirmware(std::string firmware);
    void setName(std::string name);
    void setUrl(std::string url);
    void setLogin(std::string login);
    void setPassword(std::string password);
    void setSharedId(std::string sharedId);
    void setLogicalId(std::string logicalId);
    void setChannelNumber(int channelNumber);

private:
    std::string m_id;
    std::string m_vendor;
    std::string m_model;
    std::string m_firmware;
    std::string m_name;
    std::string m_url;
    std::string m_login;
    std::string m_password;
    std::string m_sharedId;
    std::string m_logicalId;
    int m_channelNumber = 0;
};

} // namespace nx::sdk
