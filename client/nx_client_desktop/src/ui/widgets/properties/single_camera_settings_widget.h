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
class QnCameraMotionMaskWidget;
class QnImageProvider;
class CameraAdvancedSettingsWebPage;

class QnSingleCameraSettingsWidget : public Connective<QWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    typedef Connective<QWidget> base_type;

public:
    explicit QnSingleCameraSettingsWidget(QWidget *parent = NULL);
    virtual ~QnSingleCameraSettingsWidget();

    const QnVirtualCameraResourcePtr &camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

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

private:
    void setHasDbChanges(bool hasChanges);

    void updateMotionWidgetFromResource();
    void submitMotionWidgetToResource();

    void updateMotionWidgetNeedControlMaxRect();
    void updateMotionCapabilities();

    void disconnectFromMotionWidget();
    void connectToMotionWidget();

    void showMaxFpsWarningIfNeeded();

    int tabIndex(Qn::CameraSettingsTab tab) const;
    void setTabEnabledSafe(Qn::CameraSettingsTab tab, bool enabled);

    void retranslateUi();

private:
    Q_DISABLE_COPY(QnSingleCameraSettingsWidget)

    QScopedPointer<Ui::SingleCameraSettingsWidget> ui;

    QnVirtualCameraResourcePtr m_camera;
    bool m_cameraSupportsMotion;

    bool m_hasDbChanges;

    /** Indicates that the user changed motion sensitivity controls but not applied them */
    bool m_hasMotionControlsChanges;

    bool m_readOnly;

    bool m_updating;

    QnCameraMotionMaskWidget *m_motionWidget;
    QVBoxLayout *m_motionLayout;

    QHash<QnUuid, QnImageProvider*> m_imageProvidersByResourceId;

    QButtonGroup* m_sensitivityButtons;

    QString m_recordingAlert;
    QString m_motionAlert;
    bool m_lockedMode = false;
};
