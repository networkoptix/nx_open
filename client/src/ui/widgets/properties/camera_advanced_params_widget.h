#pragma once

#include <QtWidgets/QWidget>

#include <api/api_fwd.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_advanced_param.h>

#include <utils/common/connective.h>

namespace Ui {
    class CameraAdvancedParamsWidget;
}

class QnCachingCameraAdvancedParamsReader;
class QnCameraAdvancedParamWidgetsManager;

class QnCameraAdvancedParamsWidget: public Connective<QWidget> {
    Q_OBJECT
    typedef Connective<QWidget> base_type;
public:
    QnCameraAdvancedParamsWidget(QWidget* parent = NULL);
    virtual ~QnCameraAdvancedParamsWidget();

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    void loadValues();
    void saveValues();

    bool hasChanges() const;

signals:
    void hasChangesChanged();

private:
    void initSplitter();

    void initialize();
    void displayParams();
    
    void saveSingleValue(const QnCameraAdvancedParamValue &value);  

    bool isCameraAvailable() const;
    void updateCameraAvailability();
    void updateButtonsState();
    QnMediaServerConnectionPtr getServerConnection() const;

    void at_advancedParamChanged(const QString &id, const QString &value);

private slots:
    void at_advancedSettingsLoaded(int status, const QnCameraAdvancedParamValueList &params, int handle);
    void at_advancedParam_saved(int status, const QnCameraAdvancedParamValueList &params, int handle);

private:
    enum class State {
        Init,
        Loading,
        Applying
    };
    State state() const;
    void setState(State newState);

private:
    QScopedPointer<Ui::CameraAdvancedParamsWidget> ui;
    QScopedPointer<QnCachingCameraAdvancedParamsReader> m_advancedParamsReader;
    QScopedPointer<QnCameraAdvancedParamWidgetsManager> m_advancedParamWidgetsManager;
    QnVirtualCameraResourcePtr m_camera;
    bool m_cameraAvailable;
    int m_paramRequestHandle;
    State m_state;
    QnCameraAdvancedParamValueMap m_loadedValues;
    QnCameraAdvancedParamValueMap m_currentValues;
};
