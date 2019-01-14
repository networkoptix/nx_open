#pragma once

#include <QtWidgets/QWidget>

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

namespace Ui { class CameraAdvancedSettingsWidget; }

namespace nx::vms::client::desktop {

class CameraAdvancedSettingsWidget:
    public Connective<QWidget>,
    public QnConnectionContextAware
{
    Q_OBJECT

    using base_type = Connective<QWidget>;

public:
    CameraAdvancedSettingsWidget(QWidget* parent = nullptr);
    virtual ~CameraAdvancedSettingsWidget() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    void updateFromResource();
    void reloadData();
    bool hasChanges() const;
    void submitToResource();

signals:
    void hasChangesChanged();

private:
    void updatePage();
    bool hasManualPage() const;

private:
    QScopedPointer<Ui::CameraAdvancedSettingsWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
};

} // namespace nx::vms::client::desktop
