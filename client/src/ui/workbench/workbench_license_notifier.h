#ifndef QN_WORKBENCH_LICENSE_NOTIFIER_H
#define QN_WORKBENCH_LICENSE_NOTIFIER_H

#include <QtCore/QObject>

#include "workbench_context_aware.h"

class QnWorkbenchLicenseNotifier: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchLicenseNotifier(QObject *parent = NULL);
    virtual ~QnWorkbenchLicenseNotifier();

private slots:
    void at_licensePool_licensesChanged();
};

#endif // QN_WORKBENCH_LICENSE_NOTIFIER_H
