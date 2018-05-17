#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/common/widgets/panel.h>

#include "../models/camera_streams_model.h"

namespace Ui { class LegacyCameraSettingsStreamsPanel; }

namespace nx {
namespace client {
namespace desktop {

class LegacyCameraSettingsStreamsPanel: public Panel
{
    Q_OBJECT
    using base_type = Panel;

public:
    explicit LegacyCameraSettingsStreamsPanel(QWidget* parent = nullptr);
    virtual ~LegacyCameraSettingsStreamsPanel() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    bool hasChanges() const;
    void updateFromResource();
    void submitToResource();

signals:
    void hasChangesChanged();

private:
    void editCameraStreams();
    void updateFields();

private:
    QScopedPointer<Ui::LegacyCameraSettingsStreamsPanel> ui;
    QnVirtualCameraResourcePtr m_camera;
    CameraStreamsModel m_model;
};

} // namespace desktop
} // namespace client
} // namespace nx
