#include "resource_property_adaptor.h"

#include <core/resource/resource.h>

#include "app_server_connection.h"

// -------------------------------------------------------------------------- //
// QnAbstractResourcePropertyAdaptor
// -------------------------------------------------------------------------- //
QnAbstractResourcePropertyAdaptor::QnAbstractResourcePropertyAdaptor(const QnResourcePtr &resource, const QString &key, QObject *parent):
    base_type(parent),
    m_resource(resource),
    m_key(key),
    m_pendingSave(false)
{
    connect(resource,   &QnResource::propertyChanged,                           this,   &QnAbstractResourcePropertyAdaptor::at_resource_propertyChanged);
    connect(this,       &QnAbstractResourcePropertyAdaptor::saveValueQueued,    this,   &QnAbstractResourcePropertyAdaptor::saveValue, Qt::QueuedConnection);
}

QnAbstractResourcePropertyAdaptor::~QnAbstractResourcePropertyAdaptor() {
    return;
}

void QnAbstractResourcePropertyAdaptor::setValue(const QVariant &value) {
    if(equals(m_value, value))
        return;

    m_value = value;
    
    saveValueLater();

    emit valueChanged();
}

void QnAbstractResourcePropertyAdaptor::loadValue() {
    QString serializedValue = m_resource->getProperty(m_key);
    if(m_serializedValue == serializedValue)
        return;

    m_serializedValue = serializedValue;
    if(!deserialize(m_serializedValue, &m_value))
        m_value = QVariant();

    emit valueChanged();
    emit valueChangedExternally();
}

void QnAbstractResourcePropertyAdaptor::saveValue() {
    QString serializedValue;
    if(!serialize(m_value, &serializedValue))
        serializedValue = QString();

    if(m_serializedValue == serializedValue)
        return;

    m_serializedValue = serializedValue;

    m_resource->setProperty(m_key, m_serializedValue);
    QnAppServerConnectionFactory::createConnection()->saveAsync(resource()->getId(), QnKvPairList() << QnKvPair(m_key, m_serializedValue));

    m_pendingSave = false;
}

void QnAbstractResourcePropertyAdaptor::saveValueLater() {
    if(m_pendingSave)
        return;

    m_pendingSave = true;
    saveValueQueued();
}

void QnAbstractResourcePropertyAdaptor::at_resource_propertyChanged(const QnResourcePtr &, const QString &key) {
    if(key == m_key)
        loadValue();
}
