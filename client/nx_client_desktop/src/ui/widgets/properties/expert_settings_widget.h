#pragma once

#include <QtWidgets/QWidget>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <client_core/connection_context_aware.h>

namespace Ui {
class CameraExpertSettingsWidget;
} // namespace Ui

class QnCameraExpertSettingsWidget:
    public QWidget,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    QnCameraExpertSettingsWidget(QWidget* parent = nullptr);
    virtual ~QnCameraExpertSettingsWidget() override;

    void updateFromResources(const QnVirtualCameraResourceList& cameras);
    void submitToResources(const QnVirtualCameraResourceList& cameras);

    bool isSecondStreamEnabled() const;
    void setSecondStreamEnabled(bool value = true);

signals:
    void dataChanged();

private slots:
    void at_dataChanged();

    void at_restoreDefaultsButton_clicked();

    void at_secondStreamQualityChanged();

    void updateControlBlock();

private:
    bool areDefaultValues() const;

    Qn::SecondStreamQuality selectedSecondStreamQuality() const;
    void setSelectedSecondStreamQuality(Qn::SecondStreamQuality value) const;

    bool isArecontCamera(const QnVirtualCameraResourcePtr &camera) const;
    bool isMdPolicyAllowedForCamera(const QnVirtualCameraResourcePtr& camera, const QString& mdPolicy) const;

    QScopedPointer<Ui::CameraExpertSettingsWidget> ui;
    bool m_updating = false;

    bool m_hasDualStreaming = false;
    bool m_qualityEditable = false;
};
