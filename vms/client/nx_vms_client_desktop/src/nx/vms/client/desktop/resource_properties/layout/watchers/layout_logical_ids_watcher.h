#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/utils/impl_ptr.h>

class QnCommonModule;

namespace nx::vms::client::desktop {

class LayoutSettingsDialogStore;

class LayoutLogicalIdsWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    LayoutLogicalIdsWatcher(
        LayoutSettingsDialogStore* store, QnCommonModule* commonModule, QObject* parent = nullptr);
    virtual ~LayoutLogicalIdsWatcher() override;

    QnLayoutResourcePtr excludedLayout() const;
    void setExcludedLayout(const QnLayoutResourcePtr& value);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
