#include "resource_property_adaptor.h"

#include <core/resource/resource.h>

#include "app_server_connection.h"

// -------------------------------------------------------------------------- //
// QnAbstractResourcePropertyAdaptor
// -------------------------------------------------------------------------- //
QnAbstractResourcePropertyAdaptor::QnAbstractResourcePropertyAdaptor(const QnResourcePtr &resource, const QString &key, QObject *parent):
    base_type(parent),
    m_resource(resource),
    m_key(key)
{
    connect(resource, &QnResource::propertyChanged, this, &QnAbstractResourcePropertyAdaptor::at_resource_propertyChanged);
}

QnAbstractResourcePropertyAdaptor::~QnAbstractResourcePropertyAdaptor() {
    return;
}

void QnAbstractResourcePropertyAdaptor::setValue(const QVariant &value) {
    if(equals(m_value, value))
        return;

    m_value = value;
    
    saveValue();

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
}

void QnAbstractResourcePropertyAdaptor::saveValue() {
    QString serializedValue;
    if(!serialize(m_value, &serializedValue))
        serializedValue = QString();

    if(m_serializedValue == serializedValue)
        return;

    m_serializedValue = serializedValue;

    m_resource->setProperty(m_key, m_serializedValue);
    ec2::AbstractECConnectionPtr connection = QnAppServerConnectionFactory::getConnection2();
    connection->getResourceManager()->save(resource()->getId(), QnKvPairList() << QnKvPair(m_key, m_serializedValue), this, &QnAbstractResourcePropertyAdaptor::at_paramsSaved);
}

void QnAbstractResourcePropertyAdaptor::at_paramsSaved(int, ec2::ErrorCode)
{

}

void QnAbstractResourcePropertyAdaptor::at_resource_propertyChanged(const QnResourcePtr &, const QString &key) {
    if(key == m_key)
        loadValue();
}
