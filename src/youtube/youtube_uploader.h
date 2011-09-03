#ifndef YOUTUBE_UPLOADER_H
#define YOUTUBE_UPLOADER_H

#include <QObject>
#include <QList>
#include <QNetworkReply>
#include <QSslError>
#include <QStringList>

class YouTubeUploader : public QObject
{
Q_OBJECT
public:
    YouTubeUploader(const QString& login, const QString& password);
    virtual ~YouTubeUploader();

    void uploadFile(const QString& filename, const QString& title, const QString& description, const QStringList& tags);
signals:
    void uploadFinished();
    void uploadFailed(QString message, int code);
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
private slots:
    void replyFinished(QNetworkReply* reply);
    void slotReadyRead();
    void slotError(QNetworkReply::NetworkError);
    void slotSslErrors(QList<QSslError>);
private:
    void upload();
    void login();
    void nextStep();
private:
    QString m_title;
    QString m_description;
    QStringList m_tags;

    enum State {State_Unauthorized, State_Authorized, State_Finished, State_Failed};
    QNetworkAccessManager *manager;
    QString m_auth;
    QString m_login;
    QString m_password;
    QString m_filename;
    State m_state;
    QString m_errorString;
    int m_errorCode;
};

class UploadCommand : public QObject
{
Q_OBJECT
public:
    UploadCommand(YouTubeUploader* manager, const QString& login, const QString& password, const QString& filename);

    void execute();

private slots:
private:
    enum { START } m_state;

    YouTubeUploader* m_manager;
    QString m_login;
    QString m_password;
    QString m_filename;
};

#endif
