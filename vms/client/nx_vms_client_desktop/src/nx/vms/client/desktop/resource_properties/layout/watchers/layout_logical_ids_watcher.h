// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/utils/impl_ptr.h>

class QnCommonMessageProcessor;

namespace nx::vms::client::desktop {

class LayoutSettingsDialogStore;

class NX_VMS_CLIENT_DESKTOP_API LayoutLogicalIdsWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    LayoutLogicalIdsWatcher(
        LayoutSettingsDialogStore* store,
        QnResourcePool* resourcePool,
        QnCommonMessageProcessor* messageProcessor,
        QObject* parent = nullptr);
    virtual ~LayoutLogicalIdsWatcher() override;

    QnLayoutResourcePtr excludedLayout() const;
    void setExcludedLayout(const QnLayoutResourcePtr& value);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
