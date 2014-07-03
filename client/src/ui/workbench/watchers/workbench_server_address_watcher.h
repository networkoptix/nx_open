#ifndef SERVER_ADDRESS_WATCHER_H
#define SERVER_ADDRESS_WATCHER_H

#include <QtCore/QObject>
#include <ui/workbench/workbench_context_aware.h>

struct QnModuleInformation;
class QnDirectModuleFinder;
class QnDirectModuleFinderHelper;

class QnWorkbenchServerAddressWatcher : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    explicit QnWorkbenchServerAddressWatcher(QObject *parent = 0);

    void setDirectModuleFinder(QnDirectModuleFinder *directModuleFinder);
    void setDirectModuleFinderHelper(QnDirectModuleFinderHelper *directModuleFinderHelper);

private slots:
    void at_directModuleFinder_moduleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress, const QUrl &url);

private:
    QnDirectModuleFinder *m_directModuleFinder;
    QnDirectModuleFinderHelper *m_directModuleFinderHelper;
    QSet<QUrl> m_urls;
};

#endif // SERVER_ADDRESS_WATCHER_H
