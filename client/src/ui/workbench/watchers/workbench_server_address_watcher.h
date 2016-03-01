#include <QtCore/QObject>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchServerAddressWatcher : public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    explicit QnWorkbenchServerAddressWatcher(QObject* parent = nullptr);

private:
    QList<QUrl> m_urls;
};
