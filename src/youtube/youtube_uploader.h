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
    YouTubeUploader(QObject *parent = 0);
    virtual ~YouTubeUploader();

    void setLogin(const QString &login);
    void setPassword(const QString &password);

    void tryAuth();

    enum Privacy {Privacy_Public, Privacy_Unlisted, Privacy_Private};

    /**
      * Upload file to youtube
      *
      * @param filename uploading file name
      * @param title file title
      * @param descruption file descruption
      * @param tags file tag list
      * @param category youtube category. To read available category list use getCategoryList method
      *
      */
    void uploadFile(const QString& filename, const QString& title, const QString& description, const QStringList& tags, const QString& category, Privacy privacy);

    inline QString fileName() const { return m_filename; }
    inline QString fileTitle() const { return m_title; }
    inline QString fileDescription() const { return m_description; }
    inline QStringList fileTags() const { return m_tags; }

    /**
    * Read youtube category list
    *
    * @return Returns category list. Each element is QPair. First: internal category name (used in uploadFile function), Second: category display name in national charset
    *
    */
    QList<QPair<QString, QString> > getCategoryList() const;

Q_SIGNALS:
    void authFailed();
    void authFinished();

    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void uploadFailed(const QString &message, int code);
    void uploadFinished();

    /**
    * Signal is emmited then category list is loaded from youtube. If function getCategoryList is called before this signal, category list is empty (not ready)
    */
    void categoryListLoaded();

private Q_SLOTS:
    void replyFinished(QNetworkReply* reply);
    void slotReadyRead();
    void slotError(QNetworkReply::NetworkError);
    void slotSslErrors(QList<QSslError>);

private:
    void upload();
    void login();
    void nextStep();
    void readCategoryList();
    void parseCategoryXml();

private:
    QString m_title;
    QString m_description;
    QStringList m_tags;
    QString m_category;
    Privacy m_privacy;
    QList<QPair<QString, QString> > m_categoryList;

    enum State {State_Unauthorized, State_Authorized, State_Finished, State_Failed};
    QNetworkAccessManager *m_manager;
    QByteArray m_auth;
    QString m_login;
    QString m_password;
    QString m_filename;
    State m_state;
    QString m_errorString;
    int m_errorCode;
    QNetworkReply* m_categoryReply;
    QByteArray m_categoryData;
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
