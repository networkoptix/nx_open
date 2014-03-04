#include "videowall_control_message.h"

const QByteArray QnVideoWallControlOperationName[] = {
    "Exit",
    "Identify",
    "ControlStarted",
    "ControlStopped",

    "ItemRoleChanged",
    "LayoutDataChanged",

    "LayoutItemAdded",
    "LayoutItemRemoved",
    "LayoutItemDataChanged",

    "ZoomLinkAdded",
    "ZoomLinkRemoved",

    "NavigatorPositionChanged",
    "NavigatorSpeedChanged",

    "SynchronizationChanged",
    "MotionSelectionChanged"
};

QDebug operator<<(QDebug dbg, const QnVideoWallControlMessage &message) {
    dbg.nospace() << "QnVideoWallControlMessage(" << QnVideoWallControlOperationName[message.operation] << ") seq:" << message[QLatin1String("sequence")];
    return dbg.space();
}
