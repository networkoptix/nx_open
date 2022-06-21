// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "health/system_health.h"

class QnSystemHealthStringsHelper: public QObject
{
    Q_OBJECT
public:
    /** Text that is used where the most short common title is required, e.g. in settings. */
    static QString messageTitle(QnSystemHealth::MessageType messageType);

    /** Text that is in notifications. */
    static QString messageText(QnSystemHealth::MessageType messageType,
        const QString& resourceName);

    /** Text that is used where the full description is required, e.g. in notification hints. */
    static QString messageTooltip(QnSystemHealth::MessageType messageType, QString resourceName);
};
