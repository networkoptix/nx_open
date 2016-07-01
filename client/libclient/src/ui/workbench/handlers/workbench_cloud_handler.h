#pragma once

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchCloudHandlerPrivate;

class QnWorkbenchCloudHandler : public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnWorkbenchCloudHandler(QObject *parent = nullptr);
    ~QnWorkbenchCloudHandler();

private:
    QScopedPointer<QnWorkbenchCloudHandlerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnWorkbenchCloudHandler)
    Q_DISABLE_COPY(QnWorkbenchCloudHandler)
};
