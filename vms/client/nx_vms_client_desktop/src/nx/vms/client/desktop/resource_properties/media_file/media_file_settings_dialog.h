// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/button_box_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class MediaFileSettingsDialog:
    public QnButtonBoxDialog,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit MediaFileSettingsDialog(QWidget* parent);
    virtual ~MediaFileSettingsDialog();

    void updateFromResource(const QnMediaResourcePtr& resource);
    void submitToResource(const QnMediaResourcePtr& resource);

private:
    Q_SLOT void handleDataChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
