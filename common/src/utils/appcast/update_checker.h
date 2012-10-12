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
    QnUpdateChecker(const QUrl& url, const QString& platform, const QString& version, QObject *parent = NULL);

    void checkForUpdates();

signals:
    void updatesAvailable(QnUpdateInfoItems updates);

private slots:
    void finished(QNetworkReply *);

private:
    QnAppCastParser m_parser;
    QNetworkAccessManager m_accessManager;
    QNetworkReply *m_reply;
    QUrl m_url;

    QnVersion m_version;
};

#endif // UPDATE_CHECKER_H_