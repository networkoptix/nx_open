#ifndef MULTIPLE_CAMERAS_SETTINGS_MODEL_H
#define MULTIPLE_CAMERAS_SETTINGS_MODEL_H

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnCameraSettingsModel;

class QnMultipleCamerasSettingsModel: public Connective<QObject> {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    enum class State {
        Disabled,
        Enabled,
        Undefined
    };

    QnMultipleCamerasSettingsModel(QObject *parent = NULL);
    ~QnMultipleCamerasSettingsModel();

    QnVirtualCameraResourceList cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    void updateFromResources();
    void submitToResources();

    State recording() const;
    void setRecording(State state);

    State audioSupported() const;
    void setAudioSupported(State state);

    State audioEnabled() const;
    void setAudioEnabled(State state);

    State fisheye() const;
    void setFisheye(State state);

    State forcedAr() const;
    void setForcedAr(State state);
    qreal forcedArValue() const;
    void setForcedArValue(qreal value);

    State forcedRotation() const;
    void setForcedRotation(State state);
    int forcedRotationValue() const;
    void setForcedRotationValue(int value);

signals:
    void recordingChanged();
    void audioSupportedChanged();
    void audioEnabledChanged();
    void fisheyeChanged();
    void forcedArChanged();
    void forcedRotationChanged();

private:

private:
    QList<QnCameraSettingsModel*> m_models;
}


#endif //MULTIPLE_CAMERAS_SETTINGS_MODEL_H
