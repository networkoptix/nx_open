#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

struct QnStartupParameters;

namespace nx::vms::utils { class SystemUri; }

namespace nx::vms::client::desktop {

class StartupActionsHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit StartupActionsHandler(QObject* parent = nullptr);
    virtual ~StartupActionsHandler() override;

private:
    void handleUserLoggedIn(const QnUserResourcePtr& user);
    void submitDelayedDrops();

    void handleStartupParameters();
    void handleInstantDrops(const QnResourceList& resources);
    void handleAcsModeResources(const QnResourceList& resources, const qint64 timeStampMs);

    bool connectUsingCustomUri(const nx::vms::utils::SystemUri& uri);
    bool connectUsingCommandLineAuth(const QnStartupParameters& startupParameters);

    /**
     * Connects to the target system if needed.
     * @param startupParameters Client command line parameters.
     * @param haveInputFiles Whether some of the parameters were parsed as a file.
     * @return Whether connection to the system was requsted in the command line. Does not check if
     *     the connection itself was successful.
     */
    bool connectToSystemIfNeeded(
        const QnStartupParameters& startupParameters,
        bool haveInputFiles);

    /**
     * Connects to the cloud if needed.
     * @return Whether connection to the cloud was requsted in the command line. Does not check if
     *     the connection itself was successful.
     */
    bool connectToCloudIfNeeded(const QnStartupParameters& startupParameters);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
