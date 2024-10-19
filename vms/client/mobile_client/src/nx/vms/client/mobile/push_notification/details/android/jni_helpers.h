// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <QtCore/QJniObject>
#include <QtCore/QStringList>

namespace nx::vms::client::mobile::details {

QJniObject currentActivity();

QJniObject applicationContext();

std::string makeSignature(
    const QStringList& arguments,
    const QString& result);

std::string makeVoidSignature(const QStringList& arguments);

std::string makeNoParamsVoidSignature();

struct JniSignature
{
    static const char* kContext;
    static const char* kString;
    static const char* kLong;
};

} // namespace nx::vms::client::mobile::details
