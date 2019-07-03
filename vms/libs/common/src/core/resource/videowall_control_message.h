#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QDebug>

#include <nx/fusion/model_functions_fwd.h>

#include <nx/utils/uuid.h>

class QnVideoWallControlMessage
{
    Q_GADGET
    Q_ENUMS(Operation)
public:
    enum Operation {
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
        NavigatorPlayingChanged,
        NavigatorSpeedChanged,

        SynchronizationChanged,
        MotionSelectionChanged,
        MediaDewarpingParamsChanged,

        RadassModeChanged,

        EnterFullscreen,
        ExitFullscreen,

        Undefined,
        Count
    };

    QnVideoWallControlMessage() = default;
    QnVideoWallControlMessage(Operation operation):
        operation(operation){}

    QnUuid videoWallGuid;
    QnUuid instanceGuid;
    Operation operation = Operation::Undefined;
    QHash<QString, QString> params;

    QString& operator[](const QString &key) { return params[key]; }
    const QString operator[](const QString &key) const {return params[key]; }
    bool contains(const QString& key) const { return params.contains(key); }

    template<typename T>
    T value(const QString& key, const T& defaultValue = T()) const
    {
        return QnLexical::deserialized<T>(params.value(key), defaultValue);
    }

    template<typename T>
    void setValue(const QString& key, const T& value)
    {
        params.insert(key, QnLexical::serialized(value));
    }
};

QDebug operator<<(QDebug dbg, const QnVideoWallControlMessage &message);

Q_DECLARE_METATYPE(QnVideoWallControlMessage)
