#include "check_update_peer_task.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

namespace {
    const QString buildInformationSuffix = lit("update");
}

QnCheckUpdatePeerTask::QnCheckUpdatePeerTask(QObject *parent) :
    QnNetworkPeerTask(parent),
    m_networkAccessManager(new QNetworkAccessManager(this))
{
}

void QnCheckUpdatePeerTask::setBuildNumber(int buildNumber) {
    m_buildNumber = buildNumber;
}

void QnCheckUpdatePeerTask::setUpdateLocationPrefix(const QString &updateLocationPrefix) {
    m_updateLocationPrefix = updateLocationPrefix;
}

void QnCheckUpdatePeerTask::doStart() {
    QUrl url(m_updateLocationPrefix + QString::number(m_version.build()) + lit("/") + buildInformationSuffix);
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, &QnCheckUpdatePeerTask::at_reply_finished);
}

void QnCheckUpdatePeerTask::at_reply_finished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        finish(InternetError);
        return;
    }

    QByteArray data = reply->readAll();
    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();

    m_version = QnSoftwareVersion(map.value(lit("version")).toString());

    if (m_version.isNull()) {
        finish(CheckError);
        return;
    }

    QString urlPrefix = m_updateLocationPrefix + QString::number(m_buildNumber) + lit("/");

    QVariantMap packages = map.value(lit("packages")).toMap();
    for (auto platform = packages.begin(); platform != packages.end(); ++platform) {
        QVariantMap architectures = platform.value().toMap();
        for (auto arch = architectures.begin(); arch != architectures.end(); ++arch) {
            // We suppose arch name does not contain '_' char. E.g. arm_isd_s2 will be split to "arm" and "isd_s2"
            QString architecture = arch.key();
            QString modification;
            int i = architecture.indexOf(QChar::fromLatin1('_'));
            if (i != -1) {
                modification = architecture.mid(i + 1);
                architecture = architecture.left(i);
            }

            QVariantMap package = arch.value().toMap();
            QString fileName = package.value(lit("file")).toString();

            QUrl url = QUrl(urlPrefix + fileName);
            m_urlBySystemInformation.insert(QnSystemInformation(platform.key(), architecture, modification), url);
            m_fileSizeByUrl.insert(url, package.value(lit("size")).toLongLong());
        }
    }

    finish(NoError);
}
