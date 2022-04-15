// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

class QnCertificateStatisticsModule;
class QnControlsStatisticsModule;

namespace nx::vms::client::desktop {

class SystemContext;
class WindowContext;

/**
 * Main context of the desktop client application. Exists through all application lifetime and is
 * accessible from anywhere using `instance()` method.
 */
class ApplicationContext: public QObject
{
    Q_OBJECT

public:
    ApplicationContext(QObject* parent = nullptr);
    virtual ~ApplicationContext() override;

    /**
     * Main context of the desktop client application. Exists through all application lifetime.
     */
    static ApplicationContext* instance();

    /**
     * Context of the System we are currently connected to. Also contains local files.
    */
    SystemContext* currentSystemContext() const;

    /**
     * Contexts of the all Systems for which we have established connection.
     */
    std::vector<SystemContext*> systemContexts() const;

    /**
     * Register existing System Context. First registered is considered to be the Current Context.
     * Ownership is not passed, but Context is to be unregistered before being destroyed.
     * TODO: #sivanov Pass ownership here and emit signals on adding / deleting Contexts.
     * TODO: @sivanov Allow to change Current Context later.
     */
    void addSystemContext(SystemContext* systemContext);

    /**
     * Unregister existing system context before destroying.
     */
    void removeSystemContext(SystemContext* systemContext);

    /**
     * Main Window context.
     */
    WindowContext* mainWindowContext() const;

    /**
     * Register existing Window Context. First registered is considered to be the Main Window.
     * Ownership is not passed, but Context is to be unregistered before being destroyed.
     * TODO: #sivanov Pass ownership here and emit signals on adding / deleting Contexts.
     * TODO: @sivanov Allow to change Main Window later.
     */
    void addWindowContext(WindowContext* windowContext);

    /**
     * Unregister existing Window Context before destroying.
     */
    void removeWindowContext(WindowContext* windowContext);

    QnCertificateStatisticsModule* certificateStatisticsModule() const;

    QnControlsStatisticsModule* controlsStatisticsModule() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
