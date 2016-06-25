
#include "user_recent_connection_data.h"

#include <QtCore/QSettings>

#include <nx/fusion/model_functions.h>
#include <utils/common/string.h>
#include <client_core/user_recent_connection_data.h>

namespace 
{
    const auto kXorKey = lit("thereIsSomeKeyForXorOperation");

    const auto kUrlNameTag = lit("url");
    const auto kPasswordTag = lit("password");
    const auto kSystemNameTag = lit("systemName");
    const auto kStoredPasswordTag = lit("storedPassword");
    const auto kNameTag = lit("name");
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnUserRecentConnectionData, (datastream)(eq), QnUserRecentConnectionData_Fields)

QnUserRecentConnectionData::QnUserRecentConnectionData() :
    name(),
    systemName(),
    url(),
    isStoredPassword(false)
{}

QnUserRecentConnectionData::QnUserRecentConnectionData(const QString& name,
    const QString& systemName,
    const QUrl& url,
    bool isStoredPassword) : 
    name(name),
    systemName(systemName),
    url(url),
    isStoredPassword(isStoredPassword)
{}

void QnUserRecentConnectionData::writeToSettings(QSettings* settings
    , const QnUserRecentConnectionData& data)
{
    const auto encryptedPass = xorEncrypt(data.url.password(), kXorKey);
    settings->setValue(kUrlNameTag, data.url.toString());
    settings->setValue(kPasswordTag, encryptedPass);
    settings->setValue(kSystemNameTag, data.systemName);
    settings->setValue(kStoredPasswordTag, data.isStoredPassword);
    settings->setValue(kNameTag, data.name);
}

QnUserRecentConnectionData QnUserRecentConnectionData::fromSettings(QSettings *settings)
{
    QnUserRecentConnectionData data;

    data.url = QUrl(settings->value(kUrlNameTag).toString());
    data.systemName = settings->value(kSystemNameTag).toString();
    data.isStoredPassword = settings->value(kStoredPasswordTag).toBool();
    data.name = settings->value(kNameTag).toString();

    const auto encryptedPass = settings->value(kPasswordTag).toString();
    if (!encryptedPass.isEmpty())
        data.url.setPassword(xorDecrypt(encryptedPass, kXorKey));

    return data;
}

///

QnUserRecentConnectionDataList::QnUserRecentConnectionDataList()
    : base_type()
{}

QnUserRecentConnectionDataList::~QnUserRecentConnectionDataList()
{}

int QnUserRecentConnectionDataList::getIndexByName(const QString& name) const
{
    const auto it = std::find_if(begin(), end(), [name](const QnUserRecentConnectionData &data)
    {
        return (data.name == name);
    });
    return (it == end() ? kNotFoundIndex : it - begin());
}

QnUserRecentConnectionData QnUserRecentConnectionDataList::getByName(const QString& name) const
{
    const auto index = getIndexByName(name);
    return (index == kNotFoundIndex ? QnUserRecentConnectionData() : at(index));
}

bool QnUserRecentConnectionDataList::contains(const QString& name) const
{
    return (getIndexByName(name) != kNotFoundIndex);
}

QString QnUserRecentConnectionDataList::generateUniqueName(const QString &base) const
{
    int counter = 0;
    QString uniqueName;
    QString counterString(QLatin1String("(%1)"));

    while (contains(uniqueName));
        uniqueName = base + counterString.arg(++counter);

    return uniqueName;
}

bool QnUserRecentConnectionDataList::remove(const QString &name)
{
    const auto newEnd = std::remove_if(begin(), end(), 
        [name](const QnUserRecentConnectionData &data)
    {
        return (data.name == name);
    });

    const bool removed = (newEnd != end());
    erase(newEnd, end());
    return removed;
}