#ifndef UPDATE_CHECKER_H_
#define UPDATE_CHECKER_H_

#include <QtCore/QObject>
#include <QtCore/QUrl>

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
    QnUpdateChecker(const QUrl& url, QObject* parent = nullptr);

public slots:
    void checkForUpdates();

signals:
    void updateAvailable(const QnUpdateInfo& info);

private slots:
    void at_networkReply_finished();

private:
    QNetworkAccessManager* const m_networkAccessManager;
    QUrl m_url;
};

#endif // UPDATE_CHECKER_H_
