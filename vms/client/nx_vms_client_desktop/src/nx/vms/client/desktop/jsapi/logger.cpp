// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logger.h"

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop::jsapi {

Logger::Logger(QObject* parent):
    base_type(parent)
{
}

void Logger::error(const QString& message) const
{
    NX_ERROR(this, message);
}

void Logger::warning(const QString& message) const
{
    NX_WARNING(this, message);
}

void Logger::info(const QString& message) const
{
    NX_INFO(this, message);
}

void Logger::debug(const QString& message) const
{
    NX_DEBUG(this, message);
}

void Logger::verbose(const QString& message) const
{
    NX_VERBOSE(this, message);
}

} // namespace nx::vms::client::desktop::jsapi
