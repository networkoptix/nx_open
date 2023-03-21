// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/common/system_health/message_type.h>

class NX_VMS_CLIENT_DESKTOP_API QnSystemHealthStringsHelper: public QObject
{
    Q_OBJECT
public:
    using MessageType = nx::vms::common::system_health::MessageType;

    /** Text that is used where the most short common title is required, e.g. in settings. */
    static QString messageTitle(MessageType messageType);

    /** Text that is in notifications. */
    static QString messageText(MessageType messageType, const QString& resourceName);

    /** Text that is used where the full description is required, e.g. in notification hints. */
    static QString messageTooltip(MessageType messageType, QString resourceName);
};
