// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "abstract_cloud_feature_source.h"

namespace nx { class Url; }

namespace nx::vms::client::core {

class AbstractCloudStatusWatcher;
class Settings;

/**
 * Cloud flags from GET /api/utils/settings, converted to client features. A flag named after a
 * feature gives its value. A service flag tells whether a cloud service exists: a missing service
 * disables every feature that needs it, a present one tells nothing. A feature the cloud tells
 * nothing about is enabled if it needs a cloud service and disabled otherwise. Requested at
 * construction and on every change of the cloud credentials. The features enabled by the last
 * response are cached in settings per user, so they are available right after construction; the
 * disabled ones are kept in memory only, until the next response or another user.
 */
class NX_VMS_CLIENT_CORE_API CloudServiceChecker: public AbstractCloudFeatureSource
{
    Q_OBJECT

public:
    CloudServiceChecker(AbstractCloudStatusWatcher* cloudStatusWatcher,
        Settings* settings,
        QObject* parent = nullptr);

    /** Replaces the portal of the current cloud host; for tests. */
    CloudServiceChecker(AbstractCloudStatusWatcher* cloudStatusWatcher,
        Settings* settings,
        const nx::Url& cloudPortalUrl,
        QObject* parent = nullptr);

    virtual ~CloudServiceChecker() override;

    virtual CloudFeatures enabledFeatures() const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
