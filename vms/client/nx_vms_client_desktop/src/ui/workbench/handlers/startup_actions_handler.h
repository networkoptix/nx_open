// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/workbench/workbench_context_aware.h>

struct QnStartupParameters;

namespace nx::vms::utils { struct SystemUri; }

namespace nx::vms::client::desktop {

class StartupActionsHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit StartupActionsHandler(QObject* parent = nullptr);
    virtual ~StartupActionsHandler() override;

private:
    void handleConnected();
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

    /** Tries to connect using stored previous connection info. */
    bool attemptAutoLogin();

    /**
     * Connects to the cloud if needed.
     * @return Whether connection to the cloud was requsted in the command line. Does not check if
     *     the connection itself was successful.
     */
    bool connectToCloudIfNeeded(const QnStartupParameters& startupParameters);

    /**
     * Evaluates script file, reports any errors to log.
     * @param path Path to script file.
     */
    void handleScriptFile(const QString& path);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
