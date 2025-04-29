// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/network/remote_connection_fwd.h>

namespace nx::vms::client::mobile {

class UiMessages: public QObject
{
    Q_OBJECT

public:
    static void registerQmlType();

    Q_INVOKABLE static QString incompatibleServerErrorText();
    Q_INVOKABLE static QString lockoutConnectionErrorText();
    Q_INVOKABLE static QString tooOldServerErrorText();
    Q_INVOKABLE static QString factorySystemErrorText();
    Q_INVOKABLE static QString getConnectionErrorText(
        core::RemoteConnectionErrorCode errorCode,
        const nx::vms::api::ModuleInformation& moduleInformation = {});
};

} // namespace nx::vms::client::mobile
