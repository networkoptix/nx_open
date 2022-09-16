// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/vms/client/core/common/utils/common_module_aware.h>

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class CameraWebPageWidget: public QWidget, public core::CommonModuleAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CameraWebPageWidget(
        CameraSettingsDialogStore* store,
        QWidget* parent = nullptr);

    virtual ~CameraWebPageWidget() override;

public slots:
    void cleanup();

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
