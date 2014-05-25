#ifndef CHECK_UPDATE_PEER_TASK_H
#define CHECK_UPDATE_PEER_TASK_H

#include <utils/network_peer_task.h>
#include <utils/common/system_information.h>
#include <utils/common/software_version.h>

class QNetworkAccessManager;

class QnCheckUpdatePeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError = 0,
        InternetError,
        CheckError
    };

    explicit QnCheckUpdatePeerTask(QObject *parent = 0);

    void setBuildNumber(int buildNumber);
    void setUpdateLocationPrefix(const QString &updateLocationPrefix);

protected:
    virtual void doStart() override;

private slots:
    void at_reply_finished();

private:
    int m_buildNumber;
    QString m_updateLocationPrefix;

    QNetworkAccessManager *m_networkAccessManager;

    QnSoftwareVersion m_version;
    QHash<QnSystemInformation, QUrl> m_urlBySystemInformation;
    QHash<QUrl, qint64> m_fileSizeByUrl;
};

#endif // CHECK_UPDATE_PEER_TASK_H
