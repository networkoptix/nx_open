// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log.h"

namespace nx::log {

static const QString kCompatMainLogName = QString("MAIN");

const nx::log::Tag kMainTag(QString(""));
const nx::log::Tag kHttpTag(QString("HTTP"));
const nx::log::Tag kSystemTag(QString("SYSTEM"));

std::array<QString, kLogNamesCount> getCompatLoggerNames()
{
    return {kCompatMainLogName, kHttpTag.toString(), kSystemTag.toString()};
}

std::shared_ptr<nx::log::AbstractLogger> getLogger(const LogName id)
{
    // Workaround to allow the Server to dynamically change the log level using a REST handler.
    switch (id)
    {
        case LogName::main:
            return nx::log::mainLogger();
        case LogName::http:
            return nx::log::getExactLogger(kHttpTag);
        case LogName::system:
            return nx::log::getExactLogger(kSystemTag);
    }
    NX_ASSERT(false);
    return nullptr;
}

} // namespace nx::log
