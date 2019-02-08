#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_advanced_param.h>

#include <utils/common/connective.h>
#include <nx/utils/uuid.h>


namespace Ui {
class CameraAdvancedParamsWidget;
}

class QnCachingCameraAdvancedParamsReader;

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

    bool hasItemsAvailableInOffline() const;

signals:
    void hasChangesChanged();

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
    void updateParameretsVisibility();
    QnMediaServerConnectionPtr getServerConnection() const;

    // Returns current values of all parameters that belong to the given group set.
    QnCameraAdvancedParamValueMap groupParameters(const QSet<QString>& groups) const;

    void at_advancedParamChanged(const QString &id, const QString &value);

private slots:
    void at_advancedSettingsLoaded(int status, const QnCameraAdvancedParamValueList &params, int handle);
    void at_advancedParam_saved(int status, const QnCameraAdvancedParamValueList &params, int handle);
    void at_ptzCommandProcessed(int status, const QVariant& reply, int handle);

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
    QScopedPointer<QnCachingCameraAdvancedParamsReader> m_advancedParamsReader;
    QScopedPointer<CameraAdvancedParamWidgetsManager> m_advancedParamWidgetsManager;
    QnVirtualCameraResourcePtr m_camera;
    bool m_cameraAvailable;
    int m_paramRequestHandle;
    State m_state;
    QnCameraAdvancedParamValueMap m_loadedValues;
    QnCameraAdvancedParamValueMap m_currentValues;

    // Special crutch for pan-tilt-zoom-lens control
    QnUuid m_ptzSequenceId;
    int m_ptzSequenceNumber = 1;
};

} // namespace nx::vms::client::desktop
