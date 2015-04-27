#ifndef UPDATE_CHECKER_H_
#define UPDATE_CHECKER_H_

#include <QtCore/QObject>

#include <utils/common/software_version.h>

class QNetworkAccessManager;

class QnUpdateChecker : public QObject {
    Q_OBJECT
public:
    /**
     * \param url                       Url of the update feed.
     * \param parent                    Parent object.
     */
    QnUpdateChecker(const QUrl &url, QObject *parent = NULL);

public slots:
    void checkForUpdates();

signals:
    void updateAvailable(const QnSoftwareVersion &version, const QUrl &releaseNotesUrl);

private slots:
    void at_networkReply_finished();

private:
    QNetworkAccessManager *m_networkAccessManager;
    QUrl m_url;
};

#endif // UPDATE_CHECKER_H_
