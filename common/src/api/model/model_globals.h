#ifndef QN_MODEL_GLOBALS_H
#define QN_MODEL_GLOBALS_H

#include <utils/common/enum_name_mapper.h>
#include <utils/common/lexical.h>

namespace Qn {
    QN_DEFINE_NAME_MAPPED_ENUM(PtzAction,
        ((PtzContinousMoveAction,   "continuousMove"))
        ((PtzAbsoluteMoveAction,    "absoluteMove"))
        ((PtzRelativeMoveAction,    "relativeMove"))
        ((PtzGetPositionAction,     "getPosition"))
    );

} // namespace Qn

QN_DECLARE_LEXICAL_SERIALIZATION_FUNCTIONS(Qn::PtzAction)

#endif // QN_MODEL_GLOBALS_H
