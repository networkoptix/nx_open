// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJniObject>

#include <nx/vms/client/mobile/push_notification/details/abstract_secure_storage.h>

namespace nx::vms::client::mobile::details {

class AndroidSecureStorage: public AbstractSecureStorage
{
public:
    AndroidSecureStorage(const QJniObject& context);
    virtual ~AndroidSecureStorage() = default;

    virtual std::optional<std::string> load(const std::string& key) const override;
    virtual void save(const std::string& key, const std::string& value) override;

    virtual std::optional<std::vector<std::byte>> loadFile(const std::string& key) const override;
    virtual void saveFile(const std::string& key, const std::vector<std::byte>& data) override;
    virtual void removeFile(const std::string& key) override;

private:
    QJniObject m_context;
};

} // namespace nx::vms::client::mobile::details
