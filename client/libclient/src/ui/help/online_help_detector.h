#ifndef ONLINE_HELP_DETECTOR_H
#define ONLINE_HELP_DETECTOR_H

#include <QtCore/QObject>
#include <QtNetwork/QNetworkAccessManager>

class QnOnlineHelpDetector : public QObject {
    Q_OBJECT
public:
    explicit QnOnlineHelpDetector(QObject *parent = 0);

public slots:
    void fetchHelpUrl();

signals:
    void urlFetched(const QString &helpUrl);
    void error();

private slots:
    void at_networkReply_finished();
    void at_networkReply_error();

private:
    QNetworkAccessManager *m_networkAccessManager;
};

#endif // ONLINE_HELP_DETECTOR_H
