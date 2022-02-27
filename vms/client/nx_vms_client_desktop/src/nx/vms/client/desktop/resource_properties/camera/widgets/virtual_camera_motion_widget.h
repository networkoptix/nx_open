// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/scoped_connections.h>

namespace Ui { class VirtualCameraMotionWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class Aligner;

class VirtualCameraMotionWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit VirtualCameraMotionWidget(QWidget* parent = nullptr);
    virtual ~VirtualCameraMotionWidget() override;

    void setStore(CameraSettingsDialogStore* store);

    Aligner* aligner() const;

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::VirtualCameraMotionWidget> ui;
    nx::utils::ScopedConnections m_storeConnections;
    Aligner* const m_aligner = nullptr;
};

} // namespace nx::vms::client::desktop
