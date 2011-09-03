#include "youtube_uploader.h"
#include <QFile>

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
public:
    QYoutubeUploadDevice(const QString& fileName, const QString& title, const QString& description, const QStringList& tags)
    {
        m_file.setFileName(fileName);
        m_state = State_ProcessPreffix;
        m_preffixPos = 0;
        m_postfixPos = 0;
        m_title = title;
        m_description = description;
        m_tags = tags;
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

    virtual bool seek(qint64 pos)
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
            m_preffix.append("scheme=\"http://gdata.youtube.com/schemas/2007/categories.cat\">People\r\n");
            m_preffix.append("</media:category>\r\n");
            m_preffix.append("<media:keywords>");
            for (int i = 0; i < m_tags.size(); ++i) {
                m_preffix.append(m_tags[i].toUtf8());
                if (i < m_tags.size() - 1)
                    m_preffix.append(',');
            }
            m_preffix.append("</media:keywords>\r\n");
            m_preffix.append("</media:group>\r\n");
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

    virtual qint64 writeData (const char * data, qint64 maxSize ) {
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
            upload();
            break;
        case State_Finished:
            emit uploadFinished();
            break;
        case State_Failed:
            emit uploadFailed(m_errorString, m_errorCode);
            break;
    }
}

YouTubeUploader::YouTubeUploader(const QString& login, const QString& password)
    : m_login(login),
    m_password(password),
    m_state(State_Unauthorized),
    m_errorCode(0)
{
    manager = new QNetworkAccessManager(this);
    connect(manager, SIGNAL(finished(QNetworkReply*)),
             this, SLOT(replyFinished(QNetworkReply*)));
}

YouTubeUploader::~YouTubeUploader()
{
    manager->deleteLater();
}

void YouTubeUploader::login()
{
    QNetworkRequest request;
    request.setUrl(QUrl("https://www.google.com/accounts/ClientLogin"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QByteArray postData;
    postData.append("Email=" + QUrl::toPercentEncoding(m_login));
    postData.append("&Passwd=" + QUrl::toPercentEncoding(m_password));
    postData.append("&service=youtube");
    postData.append("&source=EvePlayer");

    QNetworkReply *reply = manager->post(request, postData);
    connect(reply, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(slotError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
            this, SLOT(slotSslErrors(QList<QSslError>)));
}

void YouTubeUploader::uploadFile(const QString& filename, const QString& title, const QString& description, const QStringList& tags)
{
    m_filename = filename;
    m_title = title;
    m_description = description;
    m_tags = tags;

    m_state = State_Unauthorized;
    nextStep();
}

void YouTubeUploader::upload()
{
    QNetworkRequest request;
    request.setUrl(QUrl("http://uploads.gdata.youtube.com/feeds/api/users/default/uploads"));
    request.setRawHeader("Host","uploads.gdata.youtube.com");
    request.setRawHeader("Authorization","GoogleLogin auth=" + m_auth.toAscii());
    request.setRawHeader("GData-Version","2");
    request.setRawHeader("X-GData-Key","key=AI39si7gsWbcTytSIdi7qzc9Xr9I-uEU3ewpJJPb47HN2WRhJGIckOKKlw-Su8NwmjKZI96QTCmP2BewWrxTvHijB0M8S3qOsA");
    request.setRawHeader("Slug", m_filename.toUtf8());
    request.setRawHeader("Content-Type","multipart/related; boundary=\"f93dcbA3\"");
    request.setRawHeader("Connection","close");

    QYoutubeUploadDevice* uploader = new QYoutubeUploadDevice(m_filename, m_title, m_description, m_tags);
    if (uploader->open(QIODevice::ReadOnly)) {
        request.setRawHeader("Content-Length",QString::number(uploader->size()).toUtf8());
        QNetworkReply *reply = manager->post(request, uploader);
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
        nextStep();
    }
}

void YouTubeUploader::replyFinished(QNetworkReply* reply)
{
    int  statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    /*
    qDebug((QString("Code = ") + statusCode);
    if (reply->error() == QNetworkReply::NoError) {
        foreach(QByteArray header, reply->rawHeaderList())
        {
            qDebug(header + " = " + reply->rawHeader(header));
        }
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
            QList<QByteArray> lines = reply->readAll().split('\n');
            foreach (QByteArray line, lines)
            {
                int eqPos = line.indexOf('=');
                if (eqPos != -1 && line.left(eqPos) == "Auth")
                {
                    m_auth = line.right(line.length() - eqPos -1);
                    newState = State_Authorized;
                }
            }
        }
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
    m_errorCode = err;
    QNetworkReply* reply = dynamic_cast<QNetworkReply*> (sender());
    if (reply)
        m_errorString = reply->errorString();
    if (m_state == State_Unauthorized && m_errorString.toLower().contains("forbidden"))
        m_errorString = "Invalid user name or password";
}


void YouTubeUploader::slotSslErrors(QList<QSslError> list)
{
    m_errorCode = 1;
    if (!list.isEmpty()) {
        m_errorCode = list[0].error();
        m_errorString = list[0].errorString();
    }
}
