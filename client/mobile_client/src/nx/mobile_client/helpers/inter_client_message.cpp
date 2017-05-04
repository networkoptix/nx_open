#include "inter_client_message.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json.h>

namespace nx {
namespace client {
namespace mobile {

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(InterClientMessage, Command)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    InterClientMessage,
    (json),
    (command)(parameters),
    (optional, false))

InterClientMessage::InterClientMessage()
{
}

InterClientMessage::InterClientMessage(
    InterClientMessage::Command command,
    const QString& parameters)
    :
    command(command),
    parameters(parameters)
{
}

QString InterClientMessage::toString() const
{
    return QString::fromUtf8(QJson::serialized(*this));
}

InterClientMessage InterClientMessage::fromString(const QString& string)
{
    return QJson::deserialized<InterClientMessage>(string.toUtf8());
}

} // namespace mobile
} // namespace client
} // namespace nx
