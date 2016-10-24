
#include "local_connection_data.h"

#include <QtCore/QSettings>

#include <nx/fusion/model_functions.h>
#include <nx/utils/string.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnLocalConnectionData)(QnWeightData), (datastream)(eq)(json), _Fields)

QnLocalConnectionData::QnLocalConnectionData() :
    systemName(),
    localId(),
    url(),
    isStoredPassword(false)
{}

QnLocalConnectionData::QnLocalConnectionData(
    const QString& systemName,
    const QnUuid& localId,
    const QUrl& url,
    bool isStoredPassword)
    :
    systemName(systemName),
    localId(localId),
    url(url),
    isStoredPassword(isStoredPassword)
{}
