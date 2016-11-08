#include "local_connection_data.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnLocalConnectionData)(QnWeightData), (datastream)(eq)(json), _Fields)

QnLocalConnectionData::QnLocalConnectionData() {}

QnLocalConnectionData::QnLocalConnectionData(
    const QString& systemName,
    const QnUuid& localId,
    const QUrl& url)
    :
    systemName(systemName),
    localId(localId),
    url(url),
    password(url.password())
{
    this->url.setPassword(QString());
}

bool QnLocalConnectionData::isStoredPassword() const
{
    return !password.isEmpty();
}

QUrl QnLocalConnectionData::urlWithPassword() const
{
    auto url = this->url;
    url.setPassword(password.value());
    return url;
}
