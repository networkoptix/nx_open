
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
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES( (QnLocalConnectionData), (datastream)(eq), _Fields)

QnLocalConnectionData::QnLocalConnectionData() :
    name(),
    systemName(),
    systemId(),
    url(),
    isStoredPassword(false)
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
    isStoredPassword(isStoredPassword)
{}

void QnLocalConnectionData::writeToSettings(QSettings* settings
    , QnLocalConnectionData data)
{
    const auto encryptedPass = nx::utils::xorEncrypt(data.url.password(), kXorKey);
    data.url.setPassword(QString());
    settings->setValue(kUrlNameTag, data.url.toString());
    settings->setValue(kPasswordTag, encryptedPass);
    settings->setValue(kSystemNameTag, data.systemName);
    settings->setValue(kSystemIdTag, data.systemId);
    settings->setValue(kStoredPasswordTag, data.isStoredPassword);
    settings->setValue(kNameTag, data.name);
}

QnLocalConnectionData QnLocalConnectionData::fromSettings(QSettings *settings)
{
    QnLocalConnectionData data;

    data.url = QUrl(settings->value(kUrlNameTag).toString());
    data.systemName = settings->value(kSystemNameTag).toString();
    data.systemId = settings->value(kSystemIdTag).toString();
    data.isStoredPassword = settings->value(kStoredPasswordTag).toBool();
    data.name = settings->value(kNameTag).toString();

    const auto encryptedPass = settings->value(kPasswordTag).toString();
    if (!encryptedPass.isEmpty())
        data.url.setPassword(nx::utils::xorDecrypt(encryptedPass, kXorKey));

    return data;
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
