#include "videowall_control_message.h"

#include <utils/common/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnVideoWallControlMessage, Operation)

QDebug operator<<(QDebug dbg, const QnVideoWallControlMessage &message) {
    dbg.nospace() << "QnVideoWallControlMessage(" << QnLexical::serialized(message.operation) << ") seq:" << message[lit("sequence")];
    return dbg.space();
}
