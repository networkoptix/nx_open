#ifndef _OBJECTS_H
#define _OBJECTS_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMetaType>

class Object : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id WRITE setId)

public:
    Object();

    QString id() const;
    void setId(QString id);

    QByteArray serialize() const;
    QString toString() const;

private:
    QString m_id;
};

class Event : public Object
{
    Q_OBJECT
    Q_PROPERTY(QString code READ body WRITE setCode)
    Q_PROPERTY(QString body READ body WRITE setBody)

public:
    Event();

    QString code() const;
    void setCode(QString code);

    QString body() const;
    void setBody(QString body);

private:
    QString m_code;
    QString m_body;
};

class ResourceType : public Object
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(QString description READ description WRITE setDescription)

public:
    ResourceType();

    QString name() const;
    void setName(QString name);

    QString description() const;
    void setDescription(QString description);

private:
    QString m_name;
    QString m_description;
};

class Resource : public Object
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(QString type READ type WRITE setType)
    Q_PROPERTY(QList<Resource*>* resources READ resources WRITE setResources)

public:
    Resource();

    QString name() const;
    void setName(QString name);

    QString type() const;
    void setType(QString type);

    QList<Resource*>* resources() const;
    void setResources(QList<Resource*>* resources);

private:
    QString m_name;
    QString m_type;

    QList<Resource*>* m_resources;
};

class NetworkDevice : public Resource
{
    Q_OBJECT
    Q_PROPERTY(QString url READ url WRITE setUrl)

public:
    NetworkDevice();

    QString url() const;
    void setUrl(QString url);

private:
    QString m_url;
};

class Server : public NetworkDevice
{
    Q_OBJECT
    Q_PROPERTY(QString apiUrl READ url WRITE setApiUrl)

public:
    Server();

    QString apiUrl() const;
    void setApiUrl(QString url);

private:
    QString m_apiUrl;
};

class Camera : public NetworkDevice
{
    Q_OBJECT
    Q_PROPERTY(QString mac READ mac WRITE setMac)
    Q_PROPERTY(QString login READ login WRITE setLogin)
    Q_PROPERTY(QString password READ password WRITE setPassword)

public:
    Camera();

    QString mac() const;
    void setMac(QString mac);

    QString login() const;
    void setLogin(QString login);

    QString password() const;
    void setPassword(QString password);
private:
    QString m_mac;
    QString m_login;
    QString m_password;
};

class Layout : public Resource
{
    Q_OBJECT
    Q_PROPERTY(QString userId READ userId WRITE setUserId)

public:
    Layout();

    QString userId() const;
    void setUserId(QString userId);

private:
    QString m_userId;
};

class LayoutResource : public Object
{
    Q_OBJECT
    Q_PROPERTY(QString left READ left WRITE setLeft)
    Q_PROPERTY(QString right READ right WRITE setRight)
    Q_PROPERTY(QString top READ top WRITE setTop)
    Q_PROPERTY(QString bottom READ bottom WRITE setBottom)
    Q_PROPERTY(QString videoRectLeft READ videoRectLeft WRITE setVideoRectLeft)
    Q_PROPERTY(QString videoRectRight READ videoRectRight WRITE setVideoRectRight)
    Q_PROPERTY(QString videoRectTop READ videoRectTop WRITE setVideoRectTop)
    Q_PROPERTY(QString videoRectBottom READ videoRectLeftBottom WRITE setVideoRectLeftBottom)

public:
    LayoutResource();

    QString left() const;
    QString right() const;
    QString top() const;
    QString bottom() const;

    QString videoRectLeft() const;
    QString videoRectRight() const;
    QString videoRectTop() const;
    QString videoRectLeftBottom() const;

    void setLeft(QString arg);
    void setRight(QString arg);
    void setTop(QString arg);
    void setBottom(QString arg);
    void setVideoRectLeft(QString arg);
    void setVideoRectRight(QString arg);
    void setVideoRectTop(QString arg);
    void setVideoRectLeftBottom(QString arg);

private:
    QString m_left;
    QString m_right;
    QString m_top;
    QString m_bottom;
    QString m_videoRectLeft;
    QString m_videoRectRight;
    QString m_videoRectTop;
    QString m_videoRectBottom;
};

Q_DECLARE_METATYPE(QList<Event*>*)
Q_DECLARE_METATYPE(QList<Object*>*)
Q_DECLARE_METATYPE(QList<Camera*>*)
Q_DECLARE_METATYPE(QList<ResourceType*>*)
Q_DECLARE_METATYPE(QList<Server*>*)
Q_DECLARE_METATYPE(QList<Layout*>*)
Q_DECLARE_METATYPE(QList<Resource*>*)

#endif // _OBJECTS_H
