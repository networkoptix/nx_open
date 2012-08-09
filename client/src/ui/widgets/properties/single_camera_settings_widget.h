#ifndef CAMERA_SETTINGS_DIALOG_H
#define CAMERA_SETTINGS_DIALOG_H

#include <QtGui/QWidget>
#include <core/resource/resource_fwd.h>
#include "camera_settings_tab.h"
#include "camera_advanced_settings_widget.h"


namespace Ui {
    class SingleCameraSettingsWidget;
}

class QVBoxLayout;

class QnCameraMotionMaskWidget;

class QnSingleCameraSettingsWidget : public QWidget {
    Q_OBJECT;
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly);

    typedef QWidget base_type;

public:
    explicit QnSingleCameraSettingsWidget(QWidget *parent = NULL);
    virtual ~QnSingleCameraSettingsWidget();

    const QnVirtualCameraResourcePtr &camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

    bool isCameraActive() const;
    void setCameraActive(bool active);

    bool hasCameraChanges() const {
        return m_hasCameraChanges;
    }
    bool hasDbChanges() const {
        return m_hasDbChanges;
    }

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void updateFromResource();
    void submitToResource();

    /** Check if motion region is valid */
    bool isValidMotionRegion();

signals:
    void hasChangesChanged();
    void moreLicensesRequested();

protected:
    virtual void showEvent(QShowEvent *event) override;
    virtual void hideEvent(QHideEvent *event) override;

private slots:
    void at_tabWidget_currentChanged();
    void at_dbDataChanged();
    void at_cameraDataChanged();
    void at_cameraScheduleWidget_scheduleTasksChanged();
    void at_linkActivated(const QString &urlString);
    void at_motionTypeChanged();
    void at_motionSelectionCleared();

    void updateMaxFPS();
    void updateMotionWidgetSensitivity();

    void updateAdvancedCheckboxValue();

private:
    void setHasCameraChanges(bool hasChanges);
    void setHasDbChanges(bool hasChanges);

    void updateMotionWidgetFromResource();
    void submitMotionWidgetToResource();

    void updateMotionWidgetNeedControlMaxRect();
    void updateMotionAvailability();

    void disconnectFromMotionWidget();
    void connectToMotionWidget();

    void loadSettingsFromXml();
    bool loadSettingsFromXml(const QString& filepath, QString& error);
    bool parseCameraXml(const QDomElement &cameraXml, QString& error);
    bool parseGroupXml(const QDomElement &elementXml, const QString parentId, QString& error);
    bool parseElementXml(const QDomElement &elementXml, const QString parentId, QString& error);

private:
    Q_DISABLE_COPY(QnSingleCameraSettingsWidget);

    QScopedPointer<Ui::SingleCameraSettingsWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
    bool m_cameraSupportsMotion;

    bool m_hasCameraChanges;
    bool m_hasDbChanges;
    bool m_hasScheduleChanges;
    bool m_readOnly;

    QnCameraMotionMaskWidget *m_motionWidget;
    QVBoxLayout *m_motionLayout;
    bool m_inUpdateMaxFps;

    CameraSettings m_cameraSettings;
    WidgetsById m_widgetsById;
};

/*//
// class OnvifCameraSettingReader
//

class OnvifCameraSettingReader: public CameraSettingReader
{
    static const QString& IMAGING_GROUP_NAME;
    static const QString& MAINTENANCE_GROUP_NAME;

    OnvifCameraSettingsResp& m_settings;

public:
    OnvifCameraSettingReader(OnvifCameraSettingsResp& onvifSettings, const QString& filepath);
    virtual ~OnvifCameraSettingReader();

protected:

    virtual bool isGroupEnabled(const QString& id);
    virtual bool isParamEnabled(const QString& id, const QString& parentId);
    virtual void paramFound(const CameraSetting& value, const QString& parentId);
    virtual void cleanDataOnFail();

private:

    OnvifCameraSettingReader();
};*/

#endif // CAMERA_SETTINGS_DIALOG_H
