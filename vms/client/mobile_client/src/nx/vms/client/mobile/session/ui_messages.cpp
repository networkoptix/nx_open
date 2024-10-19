// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ui_messages.h"

#include <QtQml/QtQml>

#include <nx/vms/client/core/network/remote_connection_error.h>
#include <nx/vms/client/core/network/remote_connection_error_strings.h>

namespace {

QObject* createInstance(
    QQmlEngine* /*engine*/,
    QJSEngine* /*jsEngine*/)
{
    return new nx::vms::client::mobile::UiMessages();
}

} // namespace

namespace nx::vms::client::mobile {

void UiMessages::registerQmlType()
{
    qmlRegisterSingletonType<UiMessages>("Nx.Mobile", 1, 0, "UiMessages", createInstance);
}

QString UiMessages::incompatibleServerErrorText()
{
    return core::shortErrorDescription(core::RemoteConnectionErrorCode::customizationDiffers);
}

QString UiMessages::lockoutConnectionErrorText()
{
    return core::shortErrorDescription(core::RemoteConnectionErrorCode::userIsLockedOut);
}

QString UiMessages::tooOldServerErrorText()
{
    return core::shortErrorDescription(core::RemoteConnectionErrorCode::versionIsTooLow);
}

QString UiMessages::factorySystemErrorText()
{
    return core::shortErrorDescription(core::RemoteConnectionErrorCode::factoryServer);
}

QString UiMessages::getConnectionErrorText(core::RemoteConnectionErrorCode errorCode)
{
    return core::shortErrorDescription(errorCode);
}

} // namespace nx::vms::client::mobile
