#include "videowall_control_message.h"

#include <nx/fusion/model_functions.h>

#include <nx/utils/range_adapters.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnVideoWallControlMessage, Operation)

std::string QnVideoWallControlMessage::toString() const
{
    QStringList result;
    result << QnLexical::serialized(operation);
    for (auto [key, value]: nx::utils::constKeyValueRange(params))
        result << "  [" + key + ": " + value + "]";
    return result.join('\n').toStdString();
}
