// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log.h"

namespace nx::log {

static const QString kCompatMainLogName = QString("MAIN");

const nx::utils::log::Tag kMainTag(QString(""));
const nx::utils::log::Tag kHttpTag(QString("HTTP"));
const nx::utils::log::Tag kTransactionTag(QString("TRANSACTION"));
const nx::utils::log::Tag kSystemTag(QString("SYSTEM"));
const nx::utils::log::Tag kPermissionsTag(QString("PERMISSIONS"));

std::array<QString, kLogNamesCount> getCompatLoggerNames()
{
    return {kCompatMainLogName, kHttpTag.toString(), kTransactionTag.toString(),
        kSystemTag.toString(), kPermissionsTag.toString()};
}

std::shared_ptr<nx::utils::log::AbstractLogger> getLogger(const LogName id)
{
    // Workaround to allow the Server to dynamically change the log level using a REST handler.
    switch (id)
    {
        case LogName::main:
            return nx::utils::log::mainLogger();
        case LogName::http:
            return nx::utils::log::getExactLogger(kHttpTag);
        case LogName::transaction:
            return nx::utils::log::getExactLogger(kTransactionTag);
        case LogName::system:
            return nx::utils::log::getExactLogger(kSystemTag);
        case LogName::permissions:
            return nx::utils::log::getExactLogger(kPermissionsTag);
    }
    NX_ASSERT(false);
    return nullptr;
}

} // namespace nx::log
