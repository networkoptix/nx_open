#ifndef UPDATE_CHECKER_H_
#define UPDATE_CHECKER_H_

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <nx/vms/api/data/software_version.h>

class QNetworkAccessManager;

struct QnUpdateInfo;

class QnUpdateChecker: public QObject
{
    Q_OBJECT
public:
    /**
     * \param url                       Url of the update feed.
     * \param parent                    Parent object.
     */
    QnUpdateChecker(
        const QUrl& url, 
        QObject* parent, 
        const nx::vms::api::SoftwareVersion& engineVersion);

public slots:
    void checkForUpdates();

signals:
    void updateAvailable(const QnUpdateInfo& info);

private slots:
    void at_networkReply_finished();

private:
    QNetworkAccessManager* const m_networkAccessManager;
    QUrl m_url;
    const nx::vms::api::SoftwareVersion m_engineVersion;
};

#endif // UPDATE_CHECKER_H_
