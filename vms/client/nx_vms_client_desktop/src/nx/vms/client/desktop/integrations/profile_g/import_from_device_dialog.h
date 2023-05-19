// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::integrations {

class OverlappedIdStore;

class ImportFromDeviceDialog: public QmlDialogWrapper, public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    explicit ImportFromDeviceDialog(QWidget* parent = nullptr);
    virtual ~ImportFromDeviceDialog() override;

private:
    Q_INVOKABLE void updateFilter();

private:
    struct Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop::integrations
