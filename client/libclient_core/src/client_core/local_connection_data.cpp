#include "local_connection_data.h"

#include <QtCore/QUrl>

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnLocalConnectionData)(QnWeightData), (eq)(json), _Fields)

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
{}
