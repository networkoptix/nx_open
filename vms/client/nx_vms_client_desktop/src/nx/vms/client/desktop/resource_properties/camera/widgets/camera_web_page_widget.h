// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class CameraWebPageWidget:
    public QWidget,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CameraWebPageWidget(
        SystemContext* systemContext,
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
