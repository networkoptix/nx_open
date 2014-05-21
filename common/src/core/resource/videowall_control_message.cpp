#include "videowall_control_message.h"

#include <utils/common/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    QnVideoWallControlMessage::Operation, 
    (QnVideoWallControlMessage::Exit,                           "Exit")
    (QnVideoWallControlMessage::Identify,                       "Identify")

    (QnVideoWallControlMessage::ControlStarted,                 "ControlStarted")
    (QnVideoWallControlMessage::ControlStopped,                 "ControlStopped")

    (QnVideoWallControlMessage::ItemRoleChanged,                "ItemRoleChanged")
    (QnVideoWallControlMessage::LayoutDataChanged,              "LayoutDataChanged")

    (QnVideoWallControlMessage::LayoutItemAdded,                "LayoutItemAdded")
    (QnVideoWallControlMessage::LayoutItemRemoved,              "LayoutItemRemoved")
    (QnVideoWallControlMessage::LayoutItemDataChanged,          "LayoutItemDataChanged")

    (QnVideoWallControlMessage::ZoomLinkAdded,                  "ZoomLinkAdded")
    (QnVideoWallControlMessage::ZoomLinkRemoved,                "ZoomLinkRemoved")

    (QnVideoWallControlMessage::NavigatorPositionChanged,       "NavigatorPositionChanged")
    (QnVideoWallControlMessage::NavigatorSpeedChanged,          "NavigatorSpeedChanged")

    (QnVideoWallControlMessage::SynchronizationChanged,         "SynchronizationChanged")
    (QnVideoWallControlMessage::MotionSelectionChanged,         "MotionSelectionChanged")
    (QnVideoWallControlMessage::MediaDewarpingParamsChanged,    "MediaDewarpingParamsChanged")
    )


    QDebug operator<<(QDebug dbg, const QnVideoWallControlMessage &message) {
        dbg.nospace() << "QnVideoWallControlMessage(" << QnLexical::serialized(message.operation) << ") seq:" << message[QLatin1String("sequence")];
        return dbg.space();
}
