#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <QString>

namespace QnSystemHealth {

    enum MessageType {
        EmailIsEmpty,
        NoLicenses,
        NotDefined,

        MessageTypeCount = NotDefined
    };

    QString toString(MessageType messageType);
}


#endif // SYSTEM_HEALTH_H
