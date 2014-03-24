#ifndef VIDEOWALL_CONTROL_MESSAGE_H
#define VIDEOWALL_CONTROL_MESSAGE_H

#include <QtCore/QUuid>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QDebug>

class QnVideoWallControlMessage {
public:
    enum QnVideoWallControlOperation {
        Exit,
        Identify,

        ControlStarted,
        ControlStopped,

        ItemRoleChanged,
        LayoutDataChanged,

        LayoutItemAdded,
        LayoutItemRemoved,
        LayoutItemDataChanged,

        ZoomLinkAdded,
        ZoomLinkRemoved,

        NavigatorPositionChanged,
        NavigatorSpeedChanged,

        SynchronizationChanged,
        MotionSelectionChanged,
        MediaDewarpingParamsChanged,

        Count
    };

    QnVideoWallControlMessage() {}
    QnVideoWallControlMessage(QnVideoWallControlOperation operation):
        operation(operation){}

    QUuid videoWallGuid;
    QUuid instanceGuid;
    QnVideoWallControlOperation operation;
    QHash<QString, QString> params;

    QString& operator[](const QString &key) { return params[key]; }
    const QString operator[](const QString &key) const {return params[key]; }
};


QDebug operator<<(QDebug dbg, const QnVideoWallControlMessage &message);

Q_DECLARE_METATYPE(QnVideoWallControlMessage)

#endif // VIDEOWALL_CONTROL_MESSAGE_H
