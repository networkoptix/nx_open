#ifndef SERVER_CONNECTOR_H
#define SERVER_CONNECTOR_H

#include <QtCore/QObject>
#include <utils/common/singleton.h>

class QnModuleFinder;
struct QnModuleInformation;

class QnServerConnector : public QObject, public Singleton<QnServerConnector> {
    Q_OBJECT
public:
    explicit QnServerConnector(QnModuleFinder *moduleFinder, QObject *parent = 0);

    void addConnection(const QnModuleInformation &moduleInformation, const QUrl &url);
    void removeConnection(const QnModuleInformation &moduleInformation, const QUrl &url);
    void reconnect();

private slots:
    void at_moduleFinder_moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url);
    void at_moduleFinder_moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url);
    void at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation);

private:
    QnModuleFinder *m_moduleFinder;
    QHash<QUrl, QString> m_usedUrls;
};

#endif // SERVER_CONNECTOR_H
