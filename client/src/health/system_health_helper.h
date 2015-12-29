#pragma once

#include "health/system_health.h"

class QnSystemHealthStringsHelper: public QObject {
    Q_OBJECT
public:
    /** Text that is used where the most short common title is required, e.g. in settings. */
    static QString messageTitle(QnSystemHealth::MessageType messageType);

    /** Text that is used where the short title is required, e.g. in notifications. */
    static QString messageName(QnSystemHealth::MessageType messageType, QString resourceName = QString());

    /** Text that is used where the full description is required, e.g. in notification hints. */
    static QString messageDescription(QnSystemHealth::MessageType messageType, QString resourceName);
};
