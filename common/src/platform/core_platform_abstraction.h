#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>

#include "notification/platform_notifier.h"
#include "process/platform_process.h"

class QnCorePlatformAbstraction: public QObject, public Singleton<QnCorePlatformAbstraction> {
    Q_OBJECT
public:
    QnCorePlatformAbstraction(QObject *parent = NULL);
    virtual ~QnCorePlatformAbstraction();

    QnPlatformNotifier *notifier() const {
        return m_notifier;
    }
    
    QnPlatformProcess *process(QProcess *source = NULL) const;

private:
    QnPlatformNotifier *m_notifier;
    QnPlatformProcess *m_process;
};

#define qnPlatform (QnCorePlatformAbstraction::instance())
