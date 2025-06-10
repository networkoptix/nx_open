// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <network/cloud_system_data.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

class CloudCrossSystemContext;

/**
 * Monitors available Cloud Systems and manages corresponding cross-system Contexts.
 */
class NX_VMS_CLIENT_CORE_API CloudCrossSystemManager: public QObject
{
    Q_OBJECT

public:
    explicit CloudCrossSystemManager(QObject* parent = nullptr);
    virtual ~CloudCrossSystemManager() override;

    QStringList cloudSystems() const;
    CloudCrossSystemContext* systemContext(const QString& systemId) const;

    /** Whether connections with all systems are immediately initiated. */
    // TODO: To be removed in VMS-54744.
    bool connectingAutomatically() const;
    void setConnectingAutomatically(bool value);

    void resetCloudSystems(bool enableCloudSystems);

signals:
    void systemFound(const QString& systemId);
    void systemAboutToBeLost(const QString& systemId);
    void systemLost(const QString& systemId);
    void cloudAuthorizationRequested(QPrivateSignal);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
