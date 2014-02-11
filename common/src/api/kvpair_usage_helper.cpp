#include "kvpair_usage_helper.h"

#include <api/app_server_connection.h>

#include <core/resource/resource.h>

//---------------------------------------------------------------------------//
//------------------QnKvPairUsageHelperPrivate-------------------------------//
//---------------------------------------------------------------------------//

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

//---------------------------------------------------------------------------//
//------------------QnAbstractKvPairUsageHelper------------------------------//
//---------------------------------------------------------------------------//

QnAbstractKvPairUsageHelper::QnAbstractKvPairUsageHelper(const QnResourcePtr &resource,
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

QnAbstractKvPairUsageHelper::~QnAbstractKvPairUsageHelper() {

}

QnResourcePtr QnAbstractKvPairUsageHelper::resource() const {
    return d->resource;
}

void QnAbstractKvPairUsageHelper::setResource(const QnResourcePtr &resource) {
    if (d->resource == resource)
        return;

    d->resource = resource;
    d->value = d->defaultValue;
    if (d->isValid())
        load();
}

QString QnAbstractKvPairUsageHelper::key() const {
    return d->key;
}

void QnAbstractKvPairUsageHelper::setKey(const QString &key) {
    if (d->key == key)
        return;

    d->key = key;
    d->value = d->defaultValue;
    if (d->isValid())
        load();
}

QString QnAbstractKvPairUsageHelper::innerValue() const {
    return d->value;
}

void QnAbstractKvPairUsageHelper::setInnerValue(const QString &value) {
    if (d->value == value)
        return;

    d->value = value;
    if (d->isValid())
        save();
}

void QnAbstractKvPairUsageHelper::load() {
    d->loadHandle = QnAppServerConnectionFactory::getConnection2()->getResourceManager()->getKvPairs(
        d->resource->getId(),
        this,
        &QnAbstractKvPairUsageHelper::at_connection_replyReceived);

}

void QnAbstractKvPairUsageHelper::save() {
    QnKvPairList kvPairs;
    kvPairs.push_back(QnKvPair(d->key, d->value));

    d->saveHandle = QnAppServerConnectionFactory::getConnection2()->getResourceManager()->save(
        d->resource->getId(),
        kvPairs,
        this,
        &QnAbstractKvPairUsageHelper::at_connection_replyReceived);
}

void QnAbstractKvPairUsageHelper::at_connection_replyReceived(int handle, ec2::ErrorCode err, const QnKvPairListsById &kvPairs) 
{
    if(err != ec2::ErrorCode::ok) {
        qnWarning("Failed to save/load kvPairs.");
        return;
    }

    if (!d->isValid())
        return;

    if(handle == d->loadHandle) {
        QnKvPairList pairs = kvPairs[d->resource->getId()];
        foreach(const QnKvPair &pair, pairs)
            if(pair.name() == d->key && d->value != pair.value()) {
                d->value = pair.value();
                innerValueChanged(d->value);
            }
        d->loadHandle = -1;
    } else if(handle == d->saveHandle) {
        d->saveHandle = -1;
    }
}

//---------------------------------------------------------------------------//
//------------------QnStringKvPairUsageHelper-------------------------------//
//---------------------------------------------------------------------------//

QnStringKvPairUsageHelper::QnStringKvPairUsageHelper(
        const QnResourcePtr &resource,
        const QString &key,
        const QString &defaultValue,
        QObject *parent) :
    base_type(resource, key, defaultValue, parent){}

QnStringKvPairUsageHelper::~QnStringKvPairUsageHelper(){}

QString QnStringKvPairUsageHelper::value() const {
    return innerValue();
}

void QnStringKvPairUsageHelper::setValue(const QString &value) {
    setInnerValue(value);
}

void QnStringKvPairUsageHelper::innerValueChanged(const QString &value) {
    emit valueChanged(value);
}
