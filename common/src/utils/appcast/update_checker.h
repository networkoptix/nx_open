#ifndef UPDATE_CHECKER_H_
#define UPDATE_CHECKER_H_

#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "update_info.h"
#include "appcast_parser.h"

class QnUpdateChecker : public QObject {
    Q_OBJECT
public:
    /**
     * \param url                       Url of the update feed.
     * \param platform                  Platform to get updates for, e.g. <tt>"windows-x64"</tt>.
     * \param engineVersion             Current engine version.
     * \param parent                    Parent object.
     */
    QnUpdateChecker(const QUrl& url, const QString& platform, const QString& engineVersion, QObject *parent = NULL);

    void checkForUpdates();

signals:
    void updatesAvailable(QnUpdateInfoItemList updates);

private slots:
    void finished(QNetworkReply *);

private:
    QnAppCastParser m_parser;
    QNetworkAccessManager m_accessManager;
    QNetworkReply *m_reply;
    QUrl m_url;
    QnVersion m_engineVersion;
};

#endif // UPDATE_CHECKER_H_