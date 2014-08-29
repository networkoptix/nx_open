#ifndef SERVER_ADDRESS_WATCHER_H
#define SERVER_ADDRESS_WATCHER_H

#include <QtCore/QObject>
#include <ui/workbench/workbench_context_aware.h>

struct QnModuleInformation;
class QnDirectModuleFinder;
class QnModuleFinderHelper;

class QnWorkbenchServerAddressWatcher : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    explicit QnWorkbenchServerAddressWatcher(QObject *parent = 0);

    void setDirectModuleFinder(QnDirectModuleFinder *directModuleFinder);
    void setDirectModuleFinderHelper(QnModuleFinderHelper *directModuleFinderHelper);

private slots:
    void at_directModuleFinder_moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url);

private:
    QnDirectModuleFinder *m_directModuleFinder;
    QnModuleFinderHelper *m_directModuleFinderHelper;
    QSet<QUrl> m_urls;
};

#endif // SERVER_ADDRESS_WATCHER_H
