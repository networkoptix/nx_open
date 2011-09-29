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

        // Need to detect if it's string property
#if 0
        QList<Object*> *objectList = value.value<QList<Object*> *>();
        if (objectList)
        {
            foreach(Object* object, *objectList)
            {
                text << object->serialize();
            }
        }
#endif

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

Event::Event()
{
    setObjectName("event");
}

QString Event::code() const
{
    return m_code;
}

void Event::setCode(QString code)
{
    m_code = code;
}

QString Event::body() const
{
    return m_body;
}
void Event::setBody(QString body)
{
    m_body = body;
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
    : m_resources(0)
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

QList<Resource*>* Resource::resources() const
{
    return m_resources;
}

void Resource::setResources(QList<Resource*>* resources)
{
    m_resources = resources;
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

Server::Server()
{
    setObjectName("server");
}

QString Server::apiUrl() const
{
    return m_apiUrl;
}

void Server::setApiUrl(QString url)
{
    m_apiUrl = url;
}

Layout::Layout()
{
    setObjectName("layout");
}

QString Layout::userId() const
{
    return m_userId;
}

void Layout::setUserId(QString userId)
{
    m_userId = userId;
}

LayoutResource::LayoutResource()
{
    setObjectName("layoutResource");
}

QString LayoutResource::left() const
{
    return m_left;
}

QString LayoutResource::right() const
{
    return m_right;
}

QString LayoutResource::top() const
{
    return m_top;
}

QString LayoutResource::bottom() const
{
    return m_bottom;
}

QString LayoutResource::videoRectLeft() const
{
    return m_videoRectLeft;
}

QString LayoutResource::videoRectRight() const
{
    return m_videoRectRight;
}

QString LayoutResource::videoRectTop() const
{
    return m_videoRectTop;
}

QString LayoutResource::videoRectLeftBottom() const
{
    return m_videoRectBottom;
}

void LayoutResource::setLeft(QString arg)
{
    m_left = arg;
}

void LayoutResource::setRight(QString arg)
{
    m_right = arg;
}

void LayoutResource::setTop(QString arg)
{
    m_top = arg;
}

void LayoutResource::setBottom(QString arg)
{
    m_bottom = arg;
}

void LayoutResource::setVideoRectLeft(QString arg)
{
    m_videoRectLeft = arg;
}

void LayoutResource::setVideoRectRight(QString arg)
{
    m_videoRectRight = arg;
}

void LayoutResource::setVideoRectTop(QString arg)
{
    m_videoRectTop = arg;
}

void LayoutResource::setVideoRectLeftBottom(QString arg)
{
    m_videoRectBottom = arg;
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
