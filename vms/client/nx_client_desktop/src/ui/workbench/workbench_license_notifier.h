#pragma once

#include <QtCore/QObject>

#include "workbench_context_aware.h"

class QnWorkbenchLicenseNotifier: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    QnWorkbenchLicenseNotifier(QObject *parent = nullptr);
    virtual ~QnWorkbenchLicenseNotifier();

    void checkLicenses() const;
};
