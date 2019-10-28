#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>
#include <platform/core_platform_abstraction.h>
#include <platform/images/platform_images.h>
#include <platform/shortcuts/platform_shortcuts.h>
#include "notification/platform_notifier.h"

class QnPlatformAbstraction:
    public QnCorePlatformAbstraction,
    public Singleton<QnPlatformAbstraction>
{
    Q_OBJECT
    typedef QnCorePlatformAbstraction base_type;

public:
    QnPlatformAbstraction(QObject *parent = nullptr);
    virtual ~QnPlatformAbstraction();

    QnPlatformNotifier *notifier() const { return m_notifier; }
    QnPlatformImages *images() const { return m_images; }
    QnPlatformShortcuts *shortcuts() const { return m_shortcuts; }

private:
    QnPlatformNotifier *m_notifier = nullptr;
    QnPlatformImages *m_images = nullptr;
    QnPlatformShortcuts *m_shortcuts = nullptr;
};

#define qnPlatform QnPlatformAbstraction::instance()
