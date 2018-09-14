#pragma once

#include <QtWidgets/QWidget>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <client_core/connection_context_aware.h>
#include <nx/utils/uuid.h>

namespace Ui {
class CameraExpertSettingsWidget;
} // namespace Ui

class QnCameraExpertSettingsWidget:
    public QWidget,
    public QnConnectionContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    using base_type = QWidget;

public:
    QnCameraExpertSettingsWidget(QWidget* parent = nullptr);
    virtual ~QnCameraExpertSettingsWidget() override;

    void updateFromResources(const QnVirtualCameraResourceList& cameras);
    void submitToResources(const QnVirtualCameraResourceList& cameras);

    bool isSecondStreamEnabled() const;
    void setSecondStreamEnabled(bool value = true);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

signals:
    void dataChanged();

private slots:
    void at_dataChanged();

    void at_restoreDefaultsButton_clicked();

    void updateControlBlock();

    void at_generateLogicalId();

    void at_preferredPresetTypeChanged(int index);
private:
    bool areDefaultValues() const;

    bool isArecontCamera(const QnVirtualCameraResourcePtr &camera) const;
    bool isMdPolicyAllowedForCamera(const QnVirtualCameraResourcePtr& camera, const QString& mdPolicy) const;
    int generateFreeLogicalId() const;
    void updateLogicalIdControls();

    QScopedPointer<Ui::CameraExpertSettingsWidget> ui;
    bool m_updating = false;

    bool m_hasDualStreaming = false;
    bool m_hasRemoteArchiveCapability = false;
    QnUuid m_currentCameraId;
    bool m_readOnly = false;
};
