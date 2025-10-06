// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/network/http/auth_tools.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

NX_REFLECTION_ENUM_CLASS(CloudFeature,
    none = 0
);

/**
 * Monitors cloud feature flags to control access to experimental functionality.
 *
 * During development and testing of cloud features, access should be limited to specific users.
 * The cloud instance returns a list of features available to the current user, and clients
 * respond by enabling/disabling corresponding UI elements. This watcher tracks the feature
 * list and notifies about changes.
 */
class NX_VMS_CLIENT_CORE_API CloudFeaturesWatcher : public QObject
{
    Q_OBJECT

public:
    Q_DECLARE_FLAGS(CloudFeatures, CloudFeature)
    explicit CloudFeaturesWatcher(QObject* parent = nullptr);
    virtual ~CloudFeaturesWatcher() override;

    bool hasFeature(CloudFeature feature) const;

signals:
    void featuresChanged(CloudFeatures changedFeatures);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
