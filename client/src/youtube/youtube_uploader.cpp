#include "youtube_uploader.h"

#include <QtCore/QFile>
#include <QtCore/QXmlStreamReader>
#include "utils/common/log.h"

class QYoutubeUploadDevice: public QIODevice {
private:
    enum ProcessState { State_ProcessPreffix, State_ProcessFile, State_ProcessPostfix, State_Finished };

    ProcessState m_state;
    QFile m_file;
    QByteArray m_preffix;
    QByteArray m_postfix;
    int m_preffixPos;
    int m_postfixPos;
    QString m_title;
    QString m_description;
    QStringList m_tags;
    QString m_category;
    YouTubeUploader::Privacy m_privacy;
public:
    QYoutubeUploadDevice(const QString& fileName, const QString& title, const QString& description, const QStringList& tags, const QString& category,
        YouTubeUploader::Privacy privacy)
    {
        m_file.setFileName(fileName);
        m_state = State_ProcessPreffix;
        m_preffixPos = 0;
        m_postfixPos = 0;
        m_title = title;
        m_description = description;
        m_tags = tags;
        m_category = category;
        m_privacy = privacy;
    }

    virtual bool reset () {
        m_state = State_ProcessPreffix;
        m_preffixPos = 0;
        m_postfixPos = 0;
        return m_file.reset();
    }
    virtual bool isSequential () const {
        return false;
    }

    virtual bool waitForReadyRead ( int msecs ) {
        Q_UNUSED(msecs);
        return true;
    }

    virtual bool seek(qint64 /*pos*/)
    {
        return true;
    }

    virtual bool atEnd() const {
        return m_postfixPos == m_postfix.size();
    }

    virtual bool open(OpenMode mode) {
        if (m_file.open(mode)) {
            m_preffix.append("--f93dcbA3\r\n");
            m_preffix.append("Content-Type: application/atom+xml; charset=UTF-8\r\n");
            m_preffix.append("\r\n");
            m_preffix.append("<?xml version=\"1.0\"?>\r\n");
            m_preffix.append("<entry xmlns=\"http://www.w3.org/2005/Atom\"\r\n");
            m_preffix.append("xmlns:media=\"http://search.yahoo.com/mrss/\"\r\n");
            m_preffix.append("xmlns:yt=\"http://gdata.youtube.com/schemas/2007\">\r\n");
            m_preffix.append("<media:group>\r\n");
            m_preffix.append("<media:title type=\"plain\">");
            m_preffix.append(m_title.toUtf8());
            m_preffix.append("</media:title>\r\n");
            m_preffix.append("<media:description type=\"plain\">\r\n");
            m_preffix.append(m_description.toUtf8());
            m_preffix.append("\r\n");
            m_preffix.append("</media:description>\r\n");
            m_preffix.append("<media:category ");
            m_preffix.append("scheme=\"http://gdata.youtube.com/schemas/2007/categories.cat\">");
            m_preffix.append(m_category);
            m_preffix.append("\r\n");
            m_preffix.append("</media:category>\r\n");
            m_preffix.append("<media:keywords>");
            for (int i = 0; i < m_tags.size(); ++i) {
                m_preffix.append(m_tags[i].toUtf8());
                if (i < m_tags.size() - 1)
                    m_preffix.append(',');
            }
            m_preffix.append("</media:keywords>\r\n");

            if (m_privacy == YouTubeUploader::Privacy_Private) {
                m_preffix.append("<yt:private/>\r\n");
            }

            m_preffix.append("</media:group>\r\n");

            if (m_privacy == YouTubeUploader::Privacy_Unlisted) {
                m_preffix.append("<yt:accessControl action=\"list\" permission=\"denied\" />\r\n");
            }

            m_preffix.append("</entry>\r\n");
            m_preffix.append("--f93dcbA3\r\n");
            m_preffix.append("Content-Type: video/mp4\r\n");
            m_preffix.append("Content-Transfer-Encoding: binary\r\n");
            m_preffix.append("\r\n");

            m_postfix.append("\r\n");
            m_postfix.append("--f93dcbA3\r\n");
            return QIODevice::open(mode);
        }
        else
            return false;
    }

    virtual qint64 size() const
    {
        return (qint64) m_preffix.length() + m_postfix.length() + m_file.size();
    }

    virtual void close() {
        m_file.close();
    }

protected:

    virtual qint64 writeData (const char * /*data*/, qint64 /*maxSize*/ ) {
        return 0;
    }

    virtual qint64 readData (char * data, qint64 maxSize)
    {
        int rez = 0;
        if (m_state == State_ProcessPreffix) {
            int toSend = qMin(maxSize, (qint64) m_preffix.size() - m_preffixPos);
            memcpy(data, m_preffix.data() + m_preffixPos, toSend);
            m_preffixPos += toSend;
            if (m_preffixPos == m_preffix.size())
                m_state = State_ProcessFile;
            data += toSend;
            maxSize -= toSend;
            rez += toSend;
        }

        if (m_state == State_ProcessFile)
        {
            int readed = m_file.read(data, maxSize);
            if (readed < maxSize)
                m_state = State_ProcessPostfix;
            if (readed > 0) {
                data += readed;
                maxSize -= readed;
                rez += readed;
            }
        }

        if (m_state == State_ProcessPostfix) {
            int toSend = qMin(maxSize, (qint64) m_postfix.size() - m_postfixPos);
            memcpy(data, m_postfix.data() + m_postfixPos, toSend);
            m_postfixPos += toSend;
            if (m_postfixPos == m_postfix.size()) {
                m_state = State_Finished;
            }
            data += toSend;
            maxSize -= toSend;
            rez += toSend;
        }
        return rez;
    }
};

// -------------------------- YoutubeManager --------------------------------

void YouTubeUploader::nextStep()
{
    switch (m_state)
    {
        case State_Unauthorized:
            login();
            break;
        case State_Authorized:
            if (!m_filename.isEmpty()) { // special case: trying auth
                upload();
                break;
            }
            // fall through
        case State_Finished:
            emit uploadFinished();
            break;
        case State_Failed:
            emit uploadFailed(m_errorString, m_errorCode);
            break;
    }
}

YouTubeUploader::YouTubeUploader(QObject *parent)
    : QObject(parent),
      m_state(State_Unauthorized),
      m_errorCode(0),
      m_categoryReply(0)
{
    m_manager = new QNetworkAccessManager();
    connect(m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));
    readCategoryList();
}

YouTubeUploader::~YouTubeUploader()
{
    m_manager->deleteLater();
}

void YouTubeUploader::setLogin(const QString &login)
{
    m_login = login;
}

void YouTubeUploader::setPassword(const QString &password)
{
    m_password = password;
}

void YouTubeUploader::readCategoryList()
{
    QNetworkRequest request;
    QLocale locale;
    QString langStr = locale.name();
    langStr = langStr.replace(QLatin1Char('_'), QLatin1Char('-'));
    request.setUrl(QUrl(QLatin1String("http://gdata.youtube.com/schemas/2007/categories.cat?hl=") + langStr));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/x-www-form-urlencoded"));

    m_categoryReply = m_manager->get(request);
    connect(m_categoryReply, SIGNAL(error(QNetworkReply::NetworkError)),
        this, SLOT(slotError(QNetworkReply::NetworkError)));
}

void YouTubeUploader::login()
{
    QNetworkRequest request;
    request.setUrl(QUrl(QLatin1String("https://www.google.com/accounts/ClientLogin")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/x-www-form-urlencoded"));

    QByteArray postData;
    postData.append("Email=" + QUrl::toPercentEncoding(m_login));
    postData.append("&Passwd=" + QUrl::toPercentEncoding(m_password));
    postData.append("&service=youtube");
    postData.append("&source=EvePlayer");

    QNetworkReply *reply = m_manager->post(request, postData);
    connect(reply, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(slotError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
            this, SLOT(slotSslErrors(QList<QSslError>)));
}

void YouTubeUploader::tryAuth()
{
    m_filename.clear();

    m_state = State_Unauthorized;
    nextStep();
}

void YouTubeUploader::uploadFile(const QString& filename, const QString& title, const QString& description, const QStringList& tags, const QString& category, Privacy privacy)
{
    m_filename = filename;
    m_title = title;
    m_description = description;
    m_tags = tags;
    m_category = category;
    m_privacy = privacy;

    m_state = State_Unauthorized;
    nextStep();
}

void YouTubeUploader::upload()
{
    QNetworkRequest request;
    request.setUrl(QUrl(QLatin1String("http://uploads.gdata.youtube.com/feeds/api/users/default/uploads")));
    request.setRawHeader("Host","uploads.gdata.youtube.com");
    request.setRawHeader("Authorization","GoogleLogin auth=" + m_auth);
    request.setRawHeader("GData-Version","2");
    request.setRawHeader("X-GData-Key","key=AI39si7gsWbcTytSIdi7qzc9Xr9I-uEU3ewpJJPb47HN2WRhJGIckOKKlw-Su8NwmjKZI96QTCmP2BewWrxTvHijB0M8S3qOsA");
    request.setRawHeader("Slug", m_filename.toUtf8());
    request.setRawHeader("Content-Type","multipart/related; boundary=\"f93dcbA3\"");
    request.setRawHeader("Connection","close");

    QYoutubeUploadDevice* uploader = new QYoutubeUploadDevice(m_filename, m_title, m_description, m_tags, m_category, m_privacy);
    if (uploader->open(QIODevice::ReadOnly)) {
        request.setRawHeader("Content-Length",QString::number(uploader->size()).toUtf8());
        QNetworkReply *reply = m_manager->post(request, uploader);
        uploader->setParent(reply);

        connect(reply, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(slotError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
            this, SLOT(slotSslErrors(QList<QSslError>)));
        connect(reply, SIGNAL(uploadProgress(qint64, qint64)), this, SIGNAL(uploadProgress(qint64, qint64)));
    }
    else {
        m_state = State_Failed;
        m_errorString = uploader->errorString();
        m_errorCode = 0;
        nextStep();
    }
}

void YouTubeUploader::parseCategoryXml()
{
    QXmlStreamReader xml(m_categoryReply);
    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QLatin1String("category"))
            {
                QString name = xml.attributes().value(QLatin1String("term")).toString();
                QString descr = xml.attributes().value(QLatin1String("label")).toString();
                m_categoryList << QPair<QString, QString>(name, descr);
            }
        }
    }
    emit categoryListLoaded();
}

void YouTubeUploader::replyFinished(QNetworkReply* reply)
{
    if (reply == m_categoryReply) {
        parseCategoryXml();
        m_categoryReply->deleteLater();
        m_categoryReply = 0;
        return;
    }

    int  statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    /*
    qDebug((QString("Code = ") + statusCode);
    if (reply->error() == QNetworkReply::NoError) {
        foreach (const QByteArray &header, reply->rawHeaderList())
            qDebug(header + " = " + reply->rawHeader(header));
    }
    */

    State newState = State_Failed;

    if ((statusCode == 201 || statusCode == 200) && reply->error() == QNetworkReply::NoError)
    {
        if (m_state == State_Authorized)
        {
            newState = State_Finished;
        }
        else if (m_state == State_Unauthorized)
        {
            foreach (const QByteArray &line, reply->readAll().split('\n'))
            {
                if (line.startsWith("Auth="))
                {
                    m_auth = line.mid(5);
                    newState = State_Authorized;
                    break;
                }
            }
        }
    }

    if (m_state == State_Unauthorized)
    {
        if (newState == State_Authorized)
            QMetaObject::invokeMethod(this, "authFinished", Qt::QueuedConnection);
        else
            QMetaObject::invokeMethod(this, "authFailed", Qt::QueuedConnection);
    }

    m_state = newState;
    nextStep();
    reply->deleteLater();
}

void YouTubeUploader::slotReadyRead()
{
}

void YouTubeUploader::slotError(QNetworkReply::NetworkError err)
{
    if (sender() == m_categoryReply) {
        m_categoryReply->deleteLater();
        m_categoryReply = 0;
        return;
    }

    m_errorCode = err;
    if (QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender()))
        m_errorString = reply->errorString();
    if (m_state == State_Unauthorized && m_errorString.toLower().contains(QLatin1String("forbidden")))
        m_errorString = tr("Invalid user name or password");
}
void YouTubeUploader::slotSslErrors(QList<QSslError> list)
{
    QNetworkReply* reply = dynamic_cast<QNetworkReply*>(sender());
    if (reply) {
        foreach (const QSslError &error, list)
        {
            if (error.error() == QSslError::NoError)
                reply->ignoreSslErrors();
            else {
                cl_log.log("YouTube SSL error: ", error.errorString(), cl_logWARNING);
                m_errorCode = error.error();
                m_errorString = error.errorString();
            }
        }
    }
}

QList<QPair<QString, QString> > YouTubeUploader::getCategoryList() const
{
    return m_categoryList;
}
