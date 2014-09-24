#include "client_connection_data.h"

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

QString QnConnectionDataList::defaultLastUsedNameKey(){
    return lit("* Last used connection *");
}
