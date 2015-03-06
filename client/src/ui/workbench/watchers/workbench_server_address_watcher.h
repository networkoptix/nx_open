#ifndef SERVER_ADDRESS_WATCHER_H
#define SERVER_ADDRESS_WATCHER_H

#include <QtCore/QObject>
#include <ui/workbench/workbench_context_aware.h>

struct QnModuleInformation;

class QnWorkbenchServerAddressWatcher : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    explicit QnWorkbenchServerAddressWatcher(QObject *parent = 0);

private slots:
    void at_moduleFinder_moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url);
    void at_moduleFinder_moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url);
    void at_timer_timeout();

private:
    QSet<QUrl> m_urls;
    QSet<QUrl> m_onlineUrls;
};

#endif // SERVER_ADDRESS_WATCHER_H
