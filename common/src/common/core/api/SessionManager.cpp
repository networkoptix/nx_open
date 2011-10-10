#include <QDebug>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlInputSource>
#include <QSharedPointer>
#include <ios>

#include "SessionManager.h"

class QStdStreamBuf : public std::streambuf
{
public:
    QStdStreamBuf(QIODevice *dev) : std::streambuf(), m_dev(dev)
    {
        // Initialize get pointer.  This should be zero so that underflow is called upon first read.
        this->setg(0, 0, 0);
    }

protected:
virtual std::streamsize xsgetn(std::streambuf::char_type *str, std::streamsize n)
{
    return m_dev->read(str, n);
}

virtual std::streamsize xsputn(const std::streambuf::char_type *str, std::streamsize n)
{
    return m_dev->write(str, n);
}

virtual std::streambuf::pos_type seekoff(std::streambuf::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode /*__mode*/)
{
    switch(dir)
    {
        case std::ios_base::beg:
            break;
        case std::ios_base::end:
            off = m_dev->size() - off;
            break;
        case std::ios_base::cur:
            off = m_dev->pos() + off;
            break;
    }
    if(m_dev->seek(off))
        return m_dev->pos();
    else
        return std::streambuf::pos_type(std::streambuf::off_type(-1));
}
virtual std::streambuf::pos_type seekpos(std::streambuf::pos_type off, std::ios_base::openmode /*__mode*/)
{
    if(m_dev->seek(off))
        return m_dev->pos();
    else
        return std::streambuf::pos_type(std::streambuf::off_type(-1));
}

virtual std::streambuf::int_type underflow()
{
    // Read enough bytes to fill the buffer.
    std::streamsize len = sgetn(m_inbuf, sizeof(m_inbuf)/sizeof(m_inbuf[0]));

    // Since the input buffer content is now valid (or is new)
    // the get pointer should be initialized (or reset).
    setg(m_inbuf, m_inbuf, m_inbuf + len);

    // If nothing was read, then the end is here.
    if(len == 0)
        return traits_type::eof();

    // Return the first character.
    return traits_type::not_eof(m_inbuf[0]);
}


private:
    static const std::streamsize BUFFER_SIZE = 1024;
    std::streambuf::char_type m_inbuf[BUFFER_SIZE];
    QIODevice *m_dev;
};

class QStdIStream : public std::istream
{
public:
    QStdIStream(QIODevice *dev) : std::istream(m_buf = new QStdStreamBuf(dev)) {}
    virtual ~QStdIStream()
    {
        rdbuf(0);
        delete m_buf;
    }

private:
    QStdStreamBuf * m_buf;
};

SessionManager::SessionManager(const QString& host, const QString& login, const QString& password)
    : m_manager(this),
      m_host(host),
      m_port(8000),
      m_login(login),
      m_password(password),
      m_needEvents(false)

{
    QObject::connect(&m_manager, SIGNAL(finished(QNetworkReply *)),
                              SLOT(slotRequestFinished(QNetworkReply *)));
}

#if 0
void SessionManager::openEventChannel()
{
    m_needEvents = true;

    QUrl url;
    url.setScheme("http");
    url.setHost(m_host);
    url.setPort(m_port);
    url.setPath("/api/event/");

    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("Authorization", "Basic " +
                         QByteArray(QString("%1:%2").arg(m_login).arg(m_password).toAscii()).toBase64());

    QNetworkReply *reply = m_manager.get(request);
    m_requestObjectMap[reply] = "event";
}

void SessionManager::closeEventChannel()
{
    m_needEvents = false;
}

bool hasChildElements(QDomNode node)
{
    return !node.firstChildElement().isNull();
}

template<class T>
void readObjectList(QDomNode node, QList<T*>* objects)
{
    QString nodeName = node.nodeName();

    node = node.firstChild();
    while (!node.isNull())
    {
        T *object = new T();

        QDomNode attrNode = node.firstChild();
        while (!attrNode.isNull())
        {
            //qDebug() << "Property Name: " << attrNode.nodeName().toAscii();
            //qDebug() << "Property Value: " << attrNode.toElement().text();

            QByteArray propertyName = attrNode.nodeName().toAscii();

            if (hasChildElements(attrNode))
            {
                QList<T*>* children = new QList<T*>();

                readObjectList<T>(attrNode, children);
                object->setProperty(propertyName, qVariantFromValue(children));
            } else
            {
                object->setProperty(propertyName, attrNode.toElement().text());
            }

            attrNode = attrNode.nextSibling();
        }

        objects->append(object);
        node = node.nextSibling();
    }
}

template<class T>
void readObjectList(QNetworkReply *reply, QList<T*>* objects)
{
    QDomDocument doc;
    QXmlInputSource is;
    is.setData(reply->readAll());
    if (!doc.setContent(is.data()))
    {
        qDebug("Failed to setContent");
        qDebug() << is.data();

        return;
    }

    qDebug() << "Raw data: " << is.data();

    QDomNode node = doc.documentElement();
    QString nodeName = node.nodeName();
    readObjectList(node, objects);
}
#endif

void SessionManager::slotRequestFinished(QNetworkReply *reply)
{
    qDebug("slotRequestFinished");

    if (reply->error() > 0)
    {
        qDebug() << reply->errorString();
    }
    else
    {
        QString objectName;
        if (m_requestObjectMap.contains(reply))
        {
            objectName = m_requestObjectMap[reply];
            m_requestObjectMap.remove(reply);
        }

        QStdIStream is(reply);

        if (objectName == "resourceType")
        {
            typedef xsd::api::resourceTypes::resourceTypes_t T;
            QSharedPointer<T> result;

            try
            {
                result = QSharedPointer<T>(xsd::api::resourceTypes::resourceTypes (is, xml_schema::flags::dont_validate).release());
            } catch (const xml_schema::exception& e)
            {
                qDebug(e.what());
            }

            emit resourceTypesReceived((RequestId)reply, result);
        }

#if 0
        if (objectName == "event")
        {
            if (m_needEvents)
                openEventChannel();

            typedef xsd::api::events::events_t T;
            QSharedPointer<T> result;

            try
            {
                result = QSharedPointer<T>(xsd::api::events::events (is, xml_schema::flags::dont_validate).release());
            } catch (const xml_schema::exception& e)
            {
                qDebug(e.what());
            }

            emit eventsReceived(result);
        }

        // string -> xsd::api::cameras::cameras_t, xsd::api::cameras::cameras

        if (objectName == "camera")
        {
            typedef xsd::api::cameras::cameras_t T;
            QSharedPointer<T> result;

            try
            {
                result = QSharedPointer<T>(xsd::api::cameras::cameras (is, xml_schema::flags::dont_validate).release());
            } catch (const xml_schema::exception& e)
            {
                qDebug(e.what());
            }

            emit camerasReceived((RequestId)reply, result);
        } else if (objectName == "server")
        {
            typedef xsd::api::servers::servers_t T;
            QSharedPointer<T> result;

            try
            {
                result = QSharedPointer<T>(xsd::api::servers::servers (is, xml_schema::flags::dont_validate).release());
            } catch (const xml_schema::exception& e)
            {
                qDebug(e.what());
            }

            emit serversReceived((RequestId)reply, result);
        } else if (objectName == "layout")
        {
            typedef xsd::api::layouts::layouts_t T;
            QSharedPointer<T> result;

            try
            {
                result = QSharedPointer<T>(xsd::api::layouts::layouts (is, xml_schema::flags::dont_validate).release());
            } catch (const xml_schema::exception& e)
            {
                qDebug(e.what());
            }

            emit layoutsReceived((RequestId)reply, result);
        } else if (objectName == "resource")
        {
            typedef xsd::api::resources::resources_t T;
            QSharedPointer<T> result;

            try
            {
                result = QSharedPointer<T>(xsd::api::resources::resources (is, xml_schema::flags::dont_validate).release());
            } catch (const xml_schema::exception& e)
            {
                qDebug(e.what());
            }

            emit resourcesReceived((RequestId)reply, result);
        }
#endif
    }
}

RequestId SessionManager::getObjectList(QString objectName, bool tree)
{
    QUrl url;
    url.setScheme("http");
    url.setHost(m_host);
    url.setPort(m_port);
    if (tree)
    {
        url.setPath(QString("/api/%1/tree/").arg(objectName));
        url.addQueryItem("id", "3");
    }
    else
    {
        url.setPath(QString("/api/%1/").arg(objectName));
    }


    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("Authorization", "Basic " +
                         QByteArray(QString("%1:%2").arg(m_login).arg(m_password).toAscii()).toBase64());

    QNetworkReply *reply = m_manager.get(request);
    m_requestObjectMap[reply] = objectName;

    return (RequestId) reply;
}

#if 0
RequestId SessionManager::addObject(const Object& object, const QString& additionalArgs)
{
    QUrl url;
    url.setScheme("http");
    url.setHost(m_host);
    url.setPort(m_port);
    url.setPath(QString("/api/%1/").arg(object.objectName()));

    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("Authorization", "Basic " +
                         QByteArray(QString("%1:%2").arg(m_login).arg(m_password).toAscii()).toBase64());

    QByteArray body = object.serialize();
    if (!additionalArgs.isEmpty())
        body += "&" + additionalArgs.toAscii();

    QNetworkReply *reply = m_manager.post(request, body);
    m_requestObjectMap[reply] = object.objectName();

    return (RequestId) reply;
}
#endif

RequestId SessionManager::getResourceTypes()
{
    return getObjectList("resourceType", false);
}

#if 0
RequestId SessionManager::getCameras()
{
    return getObjectList("camera", false);
}

RequestId SessionManager::getLayouts()
{
    return getObjectList("layout", false);
}

RequestId SessionManager::getServers()
{
    return getObjectList("server", false);
}

RequestId SessionManager::getResources(bool tree)
{
    return getObjectList("resource", tree);
}

RequestId SessionManager::addServer(const Server& server)
{
    return addObject(server);
}

RequestId SessionManager::addLayout(const Layout& layout)
{
    return addObject(layout);
}

RequestId SessionManager::addCamera(const Camera& camera, const QString& serverId)
{
    QString additionalArgs = QString("server_id=%1").arg(serverId);

    return addObject(camera, additionalArgs);
}

#endif
