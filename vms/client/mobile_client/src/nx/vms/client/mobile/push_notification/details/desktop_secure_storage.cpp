// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_secure_storage.h"

#include <QtCore/QtGlobal>

#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)

namespace nx::vms::client::mobile {

std::optional<std::string> DesktopSecureStorage::load(const std::string& /*key*/) const
{
    return {};
}

void DesktopSecureStorage::save(const std::string& /*key*/, const std::string& /*value*/)
{
}

std::optional<std::vector<std::byte>> DesktopSecureStorage::loadFile(const std::string& /*key*/) const
{
    return {};
}

void DesktopSecureStorage::saveFile(const std::string& /*key*/, const std::vector<std::byte>& /*data*/)
{
}

void DesktopSecureStorage::removeFile(const std::string& /*key*/)
{
}

} // namespace nx::vms::client::mobile

#endif
