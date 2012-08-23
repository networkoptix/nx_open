#ifndef QN_WORKBENCH_ECS_HANDLER_H
#define QN_WORKBENCH_ECS_HANDLER_H

#if 0

#include <QtCore/QObject>

#include <api/api_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchEcsHandler: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchEcsHandler(QObject *parent = NULL);
    virtual ~QnWorkbenchEcsHandler();

protected:
    QnAppServerConnectionPtr connection() const;

private slots:
    void at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle);
    void at_resource_deleted(int status, const QByteArray &data, const QByteArray &errorString, int handle);
    void at_resources_statusSaved(int status, const QByteArray &errorString, const QnResourceList &resources, const QList<int> &oldDisabledFlags);
};

#endif


#endif // QN_WORKBENCH_ECS_HANDLER_H
