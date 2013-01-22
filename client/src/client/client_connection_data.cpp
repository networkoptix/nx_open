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

bool QnConnectionDataList::reorderByUrl(const QUrl &url){
    QUrl clean_url(url);
    clean_url.setPassword(QString());
    foreach(QnConnectionData data, *this){
        if (data.url != clean_url)
            continue;
        QList::removeOne(data);
        QList::prepend(data);
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

QString QnConnectionDataList::defaultLastUsedName(){
    return QObject::tr("* Last used connection *");
}


QString QnConnectionDataList::defaultLastUsedNameKey(){
    return QLatin1String("* Last used connection *");
}
