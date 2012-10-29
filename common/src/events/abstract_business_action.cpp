#include "abstract_business_action.h"

#include <serializer.h>
#include <parser.h>

QByteArray QnAbstractBusinessAction::serializeParams() const
{
    QJson::Serializer serializer;
    const QByteArray serialized = serializer.serialize(QVariant(m_params));
    return serialized;
}

bool QnAbstractBusinessAction::deserializeParams(const QByteArray& value)
{
    QJson::Parser parser;
    bool ok;
    QVariant result = parser.parse(value, &ok);
    if (ok)
        m_params = result.toMap();
    return ok;
}
