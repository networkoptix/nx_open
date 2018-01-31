#pragma once

#include <QtWidgets/QWidget>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <nx/utils/uuid.h>

#include "camera_settings_tab.h"

namespace Ui {
class SingleCameraSettingsWidget;
}

class QVBoxLayout;
class QnCameraThumbnailManager;

namespace nx {
namespace client {
namespace desktop {

class CameraMotionMaskWidget;
class CameraAdvancedSettingsWebPage;

class SingleCameraSettingsWidget : public Connective<QWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    typedef Connective<QWidget> base_type;

public:
    explicit SingleCameraSettingsWidget(QWidget *parent = NULL);
    virtual ~SingleCameraSettingsWidget();

    const QnVirtualCameraResourcePtr &camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    CameraSettingsTab currentTab() const;
    void setCurrentTab(CameraSettingsTab tab);

    bool hasDbChanges() const;
    bool hasAdvancedCameraChanges() const;
    bool hasChanges() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void updateFromResource(bool silent = false);
    void submitToResource();

    /** Check if motion region is valid */
    bool isValidMotionRegion();

    /** Check if second stream is enabled if there are Motion+LQ tasks in schedule. */
    bool isValidSecondStream();

    void setExportScheduleButtonEnabled(bool enabled);

    Qn::MotionType selectedMotionType() const;

    void setLockedMode(bool locked);

signals:
    void hasChangesChanged();

protected:
    virtual void showEvent(QShowEvent *event) override;
    virtual void hideEvent(QHideEvent *event) override;

private slots:
    void at_tabWidget_currentChanged();
    void at_dbDataChanged();
    void at_cameraScheduleWidget_scheduleEnabledChanged();
    void at_linkActivated(const QString &urlString);
    void at_motionTypeChanged();
    void at_resetMotionRegionsButton_clicked();
    void at_motionRegionListChanged();
    void at_fisheyeSettingsChanged();

    void updateMotionWidgetSensitivity();
    void updateMotionAlert();
    void updateIpAddressText();
    void updateWebPageText();
    void updateAlertBar();
    void updateWearableProgressVisibility();

private:
    void setHasDbChanges(bool hasChanges);

    void updateMotionWidgetFromResource();
    void submitMotionWidgetToResource();

    void updateMotionWidgetNeedControlMaxRect();
    void updateMotionCapabilities();

    void disconnectFromMotionWidget();
    void connectToMotionWidget();

    void showMaxFpsWarningIfNeeded();

    int tabIndex(CameraSettingsTab tab) const;
    void setTabEnabledSafe(CameraSettingsTab tab, bool enabled);

    void retranslateUi();

private:
    Q_DISABLE_COPY(SingleCameraSettingsWidget)

    QScopedPointer<Ui::SingleCameraSettingsWidget> ui;
    QScopedPointer<QnCameraThumbnailManager> m_cameraThumbnailManager;
    QnVirtualCameraResourcePtr m_camera;

    bool m_cameraSupportsMotion = false;
    bool m_hasDbChanges = false;
    /** Indicates that the user changed motion sensitivity controls but not applied them */
    bool m_hasMotionControlsChanges = false;
    bool m_readOnly = false;
    bool m_updating = false;

    CameraMotionMaskWidget* m_motionWidget = nullptr;
    QVBoxLayout* m_motionLayout = nullptr;
    QButtonGroup* m_sensitivityButtons;

    QString m_recordingAlert;
    QString m_motionAlert;
    bool m_lockedMode = false;
};

} // namespace desktop
} // namespace client
} // namespace nx
