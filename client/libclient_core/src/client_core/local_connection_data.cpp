
#include "local_connection_data.h"

#include <QtCore/QSettings>

#include <nx/fusion/model_functions.h>
#include <nx/utils/string.h>

namespace
{
    const auto kXorKey = lit("thereIsSomeKeyForXorOperation");

    const auto kUrlNameTag = lit("url");
    const auto kPasswordTag = lit("password");
    const auto kSystemNameTag = lit("systemName");
    const auto kSystemIdTag = lit("systemId");
    const auto kStoredPasswordTag = lit("storedPassword");
    const auto kNameTag = lit("name");
    const auto kLastConnected = lit("lastConnectedUtcMs");
    const auto kWeight = lit("weight");
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES( (QnLocalConnectionData), (datastream)(eq), _Fields)

QnLocalConnectionData::QnLocalConnectionData() :
    name(),
    systemName(),
    systemId(),
    url(),
    isStoredPassword(false),
    weight(0),
    lastConnectedUtcMs(0)
{}

QnLocalConnectionData::QnLocalConnectionData(const QString& name,
    const QString& systemName,
    const QString& systemId,
    const QUrl& url,
    bool isStoredPassword)
    :
    name(name),
    systemName(systemName),
    systemId(systemId),
    url(url),
    isStoredPassword(isStoredPassword),
    weight(0),
    lastConnectedUtcMs(0)
{}

void QnLocalConnectionData::writeToSettings(QSettings* settings) const
{
    QUrl fixedUrl = url;
    const auto encryptedPass = nx::utils::xorEncrypt(url.password(), kXorKey);
    fixedUrl.setPassword(QString());
    settings->setValue(kUrlNameTag, fixedUrl.toString());
    settings->setValue(kPasswordTag, encryptedPass);
    settings->setValue(kSystemNameTag, systemName);
    settings->setValue(kSystemIdTag, systemId);
    settings->setValue(kStoredPasswordTag, isStoredPassword);
    settings->setValue(kNameTag, name);
    settings->setValue(kWeight, weight);
    settings->setValue(kLastConnected, lastConnectedUtcMs);
}

QnLocalConnectionData QnLocalConnectionData::fromSettings(QSettings *settings)
{
    QnLocalConnectionData data;

    data.url = QUrl(settings->value(kUrlNameTag).toString());
    data.systemName = settings->value(kSystemNameTag).toString();
    data.systemId = settings->value(kSystemIdTag).toString();
    data.isStoredPassword = settings->value(kStoredPasswordTag).toBool();
    data.name = settings->value(kNameTag).toString();
    data.weight = settings->value(kWeight, QVariant::fromValue<qreal>(0)).toReal();
    data.lastConnectedUtcMs = settings->value(kLastConnected, QVariant::fromValue<qint64>(0)).toLongLong();
    const auto encryptedPass = settings->value(kPasswordTag).toString();
    if (!encryptedPass.isEmpty())
        data.url.setPassword(nx::utils::xorDecrypt(encryptedPass, kXorKey));

    return data;
}

qreal QnLocalConnectionData::calcWeight() const
{
    static const auto getDays =
        [](qint64 utcMsSinceEpoch)
    {
        const qint64 kMsInDay = 60 * 60 * 24 * 1000;
        return utcMsSinceEpoch / kMsInDay;
    };

    const auto currentTime = QDateTime::currentMSecsSinceEpoch();
    const auto penality = (getDays(currentTime) - getDays(lastConnectedUtcMs)) / 30.0;
    return std::max<qreal>((1.0 - penality) * weight, 0);
}

///

QnLocalConnectionDataList::QnLocalConnectionDataList()
    : base_type()
{}

QnLocalConnectionDataList::~QnLocalConnectionDataList()
{}

int QnLocalConnectionDataList::getIndexByName(const QString& name) const
{
    const auto it = std::find_if(begin(), end(), [name](const QnLocalConnectionData &data)
    {
        return (data.name == name);
    });
    return (it == end() ? -1 : it - begin());
}

QnLocalConnectionData QnLocalConnectionDataList::getByName(const QString& name) const
{
    const auto index = getIndexByName(name);
    return (index == -1 ? QnLocalConnectionData() : at(index));
}

bool QnLocalConnectionDataList::contains(const QString& name) const
{
    return (getIndexByName(name) != -1);
}

QString QnLocalConnectionDataList::generateUniqueName(const QString &base) const
{
    int counter = 0;
    QString uniqueName;
    QString counterString(QLatin1String("(%1)"));

    while (contains(uniqueName));
        uniqueName = base + counterString.arg(++counter);

    return uniqueName;
}

bool QnLocalConnectionDataList::remove(const QString &name)
{
    const auto newEnd = std::remove_if(begin(), end(),
        [name](const QnLocalConnectionData &data)
    {
        return (data.name == name);
    });

    const bool removed = (newEnd != end());
    erase(newEnd, end());
    return removed;
}

QnLocalConnectionDataList::WeightHash QnLocalConnectionDataList::getWeights() const
{
    WeightHash result;
    for (const auto connection : *this)
        result.insert(connection.systemId, connection.weight);
    return result;
}
