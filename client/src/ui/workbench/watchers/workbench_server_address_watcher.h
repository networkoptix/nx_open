#ifndef SERVER_ADDRESS_WATCHER_H
#define SERVER_ADDRESS_WATCHER_H

#include <QtCore/QObject>
#include <ui/workbench/workbench_context_aware.h>

struct QnModuleInformation;
class SocketAddress;

class QnWorkbenchServerAddressWatcher : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    explicit QnWorkbenchServerAddressWatcher(QObject *parent = 0);

private slots:
    void at_moduleFinder_moduleUrlFound(const QnModuleInformation &moduleInformation, const SocketAddress &address);

private:
    QSet<QUrl> m_urls;
};

#endif // SERVER_ADDRESS_WATCHER_H
