/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#include "networkoptixmodulerevealcommon.h"

#include <QtCore/QMap>
#include <QtCore/QJsonDocument>

namespace {
    const QByteArray revealRequestStr("{ magic: \"7B938F06-ACF1-45f0-8303-98AA8057739A\" }");
}

bool RevealRequest::serialize(quint8 ** const bufStart, const quint8 *bufEnd) {
    if (bufEnd - *bufStart < revealRequestStr.size())
        return false;

    memcpy(*bufStart, revealRequestStr.data(), revealRequestStr.size());
    *bufStart += revealRequestStr.size();
    return true;
}

bool RevealRequest::deserialize(const quint8 **bufStart, const quint8 *bufEnd) {
    if (bufEnd - *bufStart < revealRequestStr.size())
        return false;

    if (memcmp(*bufStart, revealRequestStr.data(), revealRequestStr.size()) != 0)
        return false;

    *bufStart += revealRequestStr.size();
    return true;
}

bool RevealResponse::serialize(quint8 **const bufStart, const quint8 *bufEnd) {
    QVariantMap map;
    map[lit("application")] = type;
    map[lit("version")] = version;
    map[lit("customization")] = customization;
    map[lit("seed")] = seed.toString();
    map[lit("systemName")] = name;
    map[lit("systemInformation")] = systemInformation;
    for (auto it = typeSpecificParameters.begin(); it != typeSpecificParameters.end(); ++it)
        map[it.key()] = it.value();

    QByteArray data = QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact);
    if (data.size() > bufEnd - *bufStart)
        return false;
    memcpy(*bufStart, data.data(), data.size());
    *bufStart += data.size();
    return true;
}

bool RevealResponse::deserialize(const quint8 **bufStart, const quint8 *bufEnd) {
    while (*bufStart < bufEnd && **bufStart != '{')
        (*bufStart)++;

    QByteArray data(reinterpret_cast<const char *>(*bufStart), bufEnd - *bufStart);

    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();
    type = map.take(lit("application")).toString();
    version = map.take(lit("version")).toString();
    systemInformation = map.take(lit("systemInformation")).toString();
    customization = map.take(lit("customization")).toString();
    name = map.take(lit("systemName")).toString();
    seed = QUuid(map.take(lit("seed")).toString());
    for (auto it = map.begin(); it != map.end(); ++it)
        typeSpecificParameters.insert(it.key(), it.value().toString());

    return !type.isEmpty() && !version.isEmpty();
}
