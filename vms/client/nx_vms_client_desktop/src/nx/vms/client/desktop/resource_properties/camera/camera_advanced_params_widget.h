#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_advanced_param.h>
#include <api/server_rest_connection_fwd.h>

#include <utils/common/connective.h>
#include <nx/utils/uuid.h>


namespace Ui {
class CameraAdvancedParamsWidget;
}

class QnRemotePtzController;

namespace nx::vms::client::desktop {

class CameraAdvancedParamWidgetsManager;

class CameraAdvancedParamsWidget: public Connective<QWidget>
{
    Q_OBJECT
        typedef Connective<QWidget> base_type;
public:
    CameraAdvancedParamsWidget(QWidget* parent = NULL);
    virtual ~CameraAdvancedParamsWidget();

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    void loadValues();
    void saveValues();

    bool hasChanges() const;

    bool shouldBeVisible() const;

signals:
    void hasChangesChanged();
    void visibilityUpdateRequested();

private:
    void initSplitter();

    void initialize();
    void displayParams();

    void saveSingleValue(const QnCameraAdvancedParameter& parameter, const QString& value);
    void sendCustomParameterCommand(
        const QnCameraAdvancedParameter& parameter, const QString& value);

    bool isCameraAvailable() const;
    void updateCameraAvailability();
    void updateButtonsState();
    void updateParametersVisibility();

    rest::QnConnectionPtr getServerConnection();

    // Returns current values of all parameters that belong to the given group set.
    QnCameraAdvancedParamValueMap groupParameters(const QSet<QString>& groups) const;

    /** Sends setCameraParam request to mediaserver. */
    bool sendSetCameraParams(const QnCameraAdvancedParamValueList& values);
    /** Sends getCameraParam request to mediaserver. */
    bool sendGetCameraParams(const QStringList& keys);

    bool sendGetCameraParamManifest();

private:
    void at_manifestLoaded(bool success, int handle, const QnCameraAdvancedParams& manifest);
    void at_advancedParamChanged(const QString &id, const QString &value);
    void at_advancedSettingsLoaded(bool success, int handle, const QnCameraAdvancedParamValueList &params);
    void at_advancedParam_saved(bool success, int handle, const QnCameraAdvancedParamValueList &params);

private:
    enum class State
    {
        Init,
        Loading,
        Applying
    };
    State state() const;
    void setState(State newState);

private:
    QScopedPointer<Ui::CameraAdvancedParamsWidget> ui;
    QScopedPointer<CameraAdvancedParamWidgetsManager> m_advancedParamWidgetsManager;
    QScopedPointer<QnRemotePtzController> m_ptzController;
    QnVirtualCameraResourcePtr m_camera;
    QnCameraAdvancedParams m_manifest;
    bool m_cameraAvailable = false;
    int m_manifestRequestHandle = 0;
    int m_paramRequestHandle = 0;
    State m_state = State::Init;
    QnCameraAdvancedParamValueMap m_loadedValues;
    QnCameraAdvancedParamValueMap m_currentValues;
};

} // namespace nx::vms::client::desktop
