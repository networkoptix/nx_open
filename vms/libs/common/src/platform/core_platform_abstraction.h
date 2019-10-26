#pragma once

#include <QtCore/QObject>

#include "notification/platform_notifier.h"
#include "process/platform_process.h"

class QnCorePlatformAbstraction: public QObject {
    Q_OBJECT
public:
    QnCorePlatformAbstraction(QObject *parent = nullptr);
    virtual ~QnCorePlatformAbstraction();

    QnPlatformProcess *process(QProcess *source = NULL) const;

private:
    QnPlatformProcess *m_process;
};
