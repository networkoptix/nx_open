#include "client_connection_data.h"

QnConnectionData::QnConnectionData():
    name(),
    url(),
    localId()
{}

QnConnectionData::QnConnectionData(const QString &name, const nx::utils::Url &url,
    const QnUuid& localId):
    name(name),
    url(url),
    localId(localId)
{}

bool QnConnectionData::operator==(const QnConnectionData &other) const
{
    return ((name == other.name) && (url == other.url));
}

bool QnConnectionData::operator!=(const QnConnectionData &other) const
{
    return !(*this == other);
}

bool QnConnectionData::isCustom() const
{
    return localId.isNull();
}

bool QnConnectionData::isValid() const
{
    return (url.isValid() && !url.isRelative() && !url.host().isEmpty());
}

//////////////////////////////////////////////////////////////////////////////////////////////////

bool QnConnectionDataList::contains(const QString &name){
    foreach(QnConnectionData data, *this){
        if (data.name != name)
            continue;
        return true;
    }
    return false;
}

QnConnectionData QnConnectionDataList::getByName(const QString &name){
    foreach(QnConnectionData data, *this){
        if (data.name == name)
            return data;
    }
    return QnConnectionData();
}

bool QnConnectionDataList::removeOne(const QString &name){
    foreach(QnConnectionData data, *this){
        if (data.name != name)
            continue;
        QList::removeOne(data);
        return true;
    }
    return false;
}

QString QnConnectionDataList::generateUniqueName(const QString &base){
    int counter = 0;
    QString uniqueName;
    QString counterString(QLatin1String("(%1)"));
    do
        uniqueName = base + counterString.arg(++counter);
    while (contains(uniqueName));
    return uniqueName;
}

int QnConnectionDataList::getIndexByName(const QString& name) const
{
    const auto it = std::find_if(begin(), end(),
        [name](const QnConnectionData& value) { return (name == value.name); });
    return (it == end() ? -1 : it - begin());
}
