#include <QUrl>

#include <QMetaProperty>
#include <QTextStream>

#include "Objects.h"

Object::Object()
{
}

QString Object::id() const
{
    return m_id;
}

void Object::setId(QString id)
{
    m_id = id;
}

QByteArray Object::serialize() const
{
    QString output;
    QTextStream text(&output);

    const QMetaObject *metaobject = metaObject();
    int count = metaobject->propertyCount();
    for (int i = 0; i < count; ++i)
    {
        QMetaProperty metaproperty = metaobject->property(i);
        const char *name = metaproperty.name();
        QVariant value = property(name);

        text << name << "=" << QUrl::toPercentEncoding(value.toString());
        if (i != count - 1)
            text << "&";
     }

     return output.toAscii();
}

QString Object::toString() const
{
    QString output;
    QTextStream text(&output);

    const QMetaObject *metaobject = metaObject();
    int count = metaobject->propertyCount();
    for (int i = 0; i < count; ++i)
    {
        QMetaProperty metaproperty = metaobject->property(i);
        const char *name = metaproperty.name();
        QVariant value = property(name);

        text << name << " = " << value.toString();
        if (i != count - 1)
            text << ", ";
     }

     return output;
}

ResourceType::ResourceType()
{
    setObjectName("resourceType");
}

QString ResourceType::name() const
{
    return m_name;
}

void ResourceType::setName(QString name)
{
    m_name = name;
}

QString ResourceType::description() const
{
    return m_description;
}

void ResourceType::setDescription(QString description)
{
    m_description = description;
}

Resource::Resource()
{
    setObjectName("resource");
}

QString Resource::name() const
{
    return m_name;
}

void Resource::setName(QString name)
{
    m_name = name;
}

QString Resource::type() const
{
    return m_type;
}

void Resource::setType(QString type)
{
    m_type = type;
}

NetworkDevice::NetworkDevice()
{
    setObjectName("networkDevice");
}

QString NetworkDevice::url() const
{
    return m_url;
}

void NetworkDevice::setUrl(QString url)
{
    m_url = url;
}

Camera::Camera()
{
    setObjectName("camera");
}

QString Camera::mac() const
{
    return m_mac;
}

void Camera::setMac(QString mac)
{
    m_mac = mac;
}

QString Camera::login() const
{
    return m_login;
}

void Camera::setLogin(QString login)
{
    m_login = login;
}

QString Camera::password() const
{
    return m_password;
}

void Camera::setPassword(QString password)
{
    m_password = password;
}
