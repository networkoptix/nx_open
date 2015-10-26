#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <QtCore/QString>
#include <QtCore/QObject>
#include "business/business_fwd.h"

namespace QnSystemHealth {
    enum MessageType {
        // These messages are generated on the client
        EmailIsEmpty,
        NoLicenses,
        SmtpIsNotSet,
        UsersEmailIsEmpty,
        ConnectionLost,
        NoPrimaryTimeServer,
        SystemIsReadOnly,

        // These messages are sent from server
        EmailSendError,
        StoragesNotConfigured,
        StoragesAreFull,
        ArchiveRebuildFinished,
        ArchiveRebuildCanceled,
        ArchiveBackupFinished,

        NotDefined,

        MessageTypeCount = ArchiveRebuildFinished
    };
}


class QnSystemHealthStringsHelper: public QObject {
    Q_OBJECT
public:
    /** Text that is used where the most short common title is required, e.g. in settings. */
    static QString messageTitle(QnSystemHealth::MessageType messageType);

    /** Text that is used where the short title is required, e.g. in notifications. */
    static QString messageName(QnSystemHealth::MessageType messageType, QString resourceName = QString());

    /** Text that is used where the full description is required, e.g. in notification hints. */
    static QString messageDescription(QnSystemHealth::MessageType messageType, const QnAbstractBusinessActionPtr &businessAction, QString resourceName);
private:
    static QString backupPositionToStr(const QDateTime& dt);
};


#endif // SYSTEM_HEALTH_H
