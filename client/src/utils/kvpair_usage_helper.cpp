#include "kvpair_usage_helper.h"

#include <api/app_server_connection.h>

#include <core/resource/resource.h>

struct QnKvPairUsageHelperPrivate {
    QnKvPairUsageHelperPrivate(): loadHandle(-1), saveHandle(-1) {}

    bool isValid() const {
        return resource && !key.isEmpty();
    }

    QnResourcePtr resource;
    QString key;
    QString value;
    QString defaultValue;
    int loadHandle;
    int saveHandle;
};


QnKvPairUsageHelper::QnKvPairUsageHelper(const QnResourcePtr &resource,
                                         const QString &key,
                                         const QString &defaultValue,
                                         QObject *parent) :
    QObject(parent),
    d(new QnKvPairUsageHelperPrivate())
{
    d->resource = resource;
    d->key = key;
    d->defaultValue = defaultValue;
    d->value = d->defaultValue;
    if (d->isValid())
        load();
}

QnKvPairUsageHelper::QnKvPairUsageHelper(const QnResourcePtr &resource,
                                         const QString &key,
                                         const quint64 &defaultValue,
                                         QObject *parent) :
    QObject(parent),
    d(new QnKvPairUsageHelperPrivate())
{
    d->resource = resource;
    d->key = key;
    d->defaultValue = QString::number(defaultValue, 16);
    d->value = d->defaultValue;
    if (d->isValid())
        load();
}

QnKvPairUsageHelper::~QnKvPairUsageHelper() {

}

QnResourcePtr QnKvPairUsageHelper::resource() const {
    return d->resource;
}

void QnKvPairUsageHelper::setResource(const QnResourcePtr &resource) {
    if (d->resource == resource)
        return;

    d->resource = resource;
    d->value = d->defaultValue;
    if (d->isValid())
        load();
}

QString QnKvPairUsageHelper::key() const {
    return d->key;
}

void QnKvPairUsageHelper::setKey(const QString &key) {
    if (d->key == key)
        return;

    d->key = key;
    d->value = d->defaultValue;
    if (d->isValid())
        load();
}

QString QnKvPairUsageHelper::value() const {
    return d->value;
}

void QnKvPairUsageHelper::setValue(const QString &value) {
    if (d->value == value)
        return;

    d->value = value;
    if (d->isValid())
        save();
}

quint64 QnKvPairUsageHelper::valueAsFlags() const {
    return d->value.toULongLong(0, 16);
}

void QnKvPairUsageHelper::setFlagsValue(quint64 value) {
    if (valueAsFlags() == value)
        return;

    d->value = QString::number(value, 16);
    if (d->isValid())
        save();
}

void QnKvPairUsageHelper::load() {
    d->loadHandle = QnAppServerConnectionFactory::createConnection()->getKvPairsAsync(
                this,
                SLOT(at_connection_replyReceived(int, const QByteArray &, const QnKvPairs &, int)));
}

void QnKvPairUsageHelper::save() {
    QnKvPairList kvPairs;
    kvPairs.push_back(QnKvPair(d->key, d->value));

    d->saveHandle = QnAppServerConnectionFactory::createConnection()->saveAsync(
                d->resource,
                kvPairs,
                this,
                SLOT(at_connection_replyReceived(int, const QByteArray &, const QnKvPairs &, int)));
}

void QnKvPairUsageHelper::at_connection_replyReceived(int status, const QByteArray &errorString, const QnKvPairs &kvPairs, int handle) {
    if(status != 0) {
        qnWarning("Failed to save/load kvPairs cause of %1.", errorString);
        return;
    }

    if (!d->isValid())
        return;

    if(handle == d->loadHandle) {
        QnKvPairList pairs = kvPairs[d->resource->getId()];
        foreach(const QnKvPair &pair, pairs)
            if(pair.name() == d->key && d->value != pair.value()) {
                d->value = pair.value();
                emit valueChanged(d->value);
            }
        d->loadHandle = -1;
    } else if(handle == d->saveHandle) {
        d->saveHandle = -1;
    }
}
