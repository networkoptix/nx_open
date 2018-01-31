#pragma once

#include <QtWidgets/QWidget>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <client_core/connection_context_aware.h>

namespace Ui {
class CameraExpertSettingsWidget;
} // namespace Ui

namespace nx {
namespace client {
namespace desktop {

class CameraExpertSettingsWidget:
    public QWidget,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    CameraExpertSettingsWidget(QWidget* parent = nullptr);
    virtual ~CameraExpertSettingsWidget() override;

    void updateFromResources(const QnVirtualCameraResourceList& cameras);
    void submitToResources(const QnVirtualCameraResourceList& cameras);

    bool isSecondStreamEnabled() const;
    void setSecondStreamEnabled(bool value = true);

signals:
    void dataChanged();

private slots:
    void at_dataChanged();

    void at_restoreDefaultsButton_clicked();

    void updateControlBlock();

private:
    bool areDefaultValues() const;

    bool isArecontCamera(const QnVirtualCameraResourcePtr &camera) const;
    bool isMdPolicyAllowedForCamera(const QnVirtualCameraResourcePtr& camera, const QString& mdPolicy) const;

    QScopedPointer<Ui::CameraExpertSettingsWidget> ui;
    bool m_updating = false;

    bool m_hasDualStreaming = false;
    bool m_hasRemoteArchiveCapability = false;
};

} // namespace desktop
} // namespace client
} // namespace nx
