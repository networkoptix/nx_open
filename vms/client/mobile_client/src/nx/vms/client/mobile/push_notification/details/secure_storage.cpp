// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "secure_storage.h"

#include <QtCore/QtGlobal>

#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)

namespace nx::vms::client::mobile {

template<>
SecureStorage::SecureStorage()
{
}

std::optional<std::string> SecureStorage::load(const std::string& /*key*/) const
{
    return {};
}

void SecureStorage::save(const std::string& /*key*/, const std::string& /*value*/)
{
}

std::optional<std::vector<std::byte>> SecureStorage::loadFile(const std::string& /*key*/) const
{
    return {};
}

void SecureStorage::saveFile(const std::string& /*key*/, const std::vector<std::byte>& /*data*/)
{
}

void SecureStorage::removeFile(const std::string& /*key*/)
{
}

} // namespace nx::vms::client::mobile

#endif
