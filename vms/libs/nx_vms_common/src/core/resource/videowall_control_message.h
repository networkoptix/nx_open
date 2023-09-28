// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/uuid.h>

class NX_VMS_COMMON_API QnVideoWallControlMessage
{
public:
    NX_REFLECTION_ENUM_IN_CLASS(Operation,
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
    )

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

    std::string toString() const;
};
