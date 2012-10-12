#include "update_checker.h"

#include <QDebug>
#include <QNetworkReply>

QnUpdateChecker::QnUpdateChecker(const QUrl& url, const QString& platform, const QString& version, QObject *parent)
    : QObject(parent),
      m_url(url),
      m_version(version) 
{
    m_parser.setPlatform(platform);
    connect(&m_accessManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(finished(QNetworkReply *)));    
}

void QnUpdateChecker::checkForUpdates() {
    QNetworkRequest request(m_url);
    m_accessManager.get(request);
}

void QnUpdateChecker::finished(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError)
        return;

    m_parser.parse(reply->readAll());

    QnUpdateInfoItems items = m_parser.newItems(m_version);
    if (!items.isEmpty())
        emit updatesAvailable(items);
}
