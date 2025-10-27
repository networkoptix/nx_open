// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/vms/client/core/system_context_aware.h>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

class SystemContext;

// TODO: Move media download backend global access check here.
class NX_VMS_CLIENT_CORE_API FeatureAccessWatcher: public QObject,
    public SystemContextAware
{
    Q_OBJECT

    using base_type = SystemContextAware;

    Q_PROPERTY(bool canUseShareBookmark
        READ canUseShareBookmark
        NOTIFY canUseShareBookmarkChanged)

    Q_PROPERTY(bool canUseDeployByQrFeature
        READ canUseDeployByQrFeature
        NOTIFY canUseDeployByQrFeatureChanged)

public:
    FeatureAccessWatcher(SystemContext* context, QObject* parent = nullptr);

    virtual ~FeatureAccessWatcher() override;

    bool canUseShareBookmark() const;

    bool canUseDeployByQrFeature() const;

signals:
    void canUseShareBookmarkChanged();

    void canUseDeployByQrFeatureChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
