#ifndef _OBJECTS_H
#define _OBJECTS_H

#include <QObject>
#include <QString>

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

public:
    Resource();

    QString name() const;
    void setName(QString name);

    QString type() const;
    void setType(QString type);

private:
    QString m_name;
    QString m_type;
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

#endif // _OBJECTS_H
