#ifndef QN_WORKBENCH_PTZ_PRESET_MANAGER_H
#define QN_WORKBENCH_PTZ_PRESET_MANAGER_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <api/model/kvpair.h>
#include <core/resource/resource_fwd.h>

#include <client/client_model_types.h>

#include "workbench_context_aware.h"

class QnWorkbenchPtzPresetManagerPrivate;

class QnWorkbenchPtzPresetManager: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchPtzPresetManager(QObject *parent = NULL);
    virtual ~QnWorkbenchPtzPresetManager();

    QnPtzPreset ptzPreset(const QnVirtualCameraResourcePtr &camera, const QString &name) const;
    QnPtzPreset ptzPreset(const QnVirtualCameraResourcePtr &camera, int hotkey) const;
    QList<QnPtzPreset> ptzPresets(const QnVirtualCameraResourcePtr &camera) const;
    void setPtzPresets(const QnVirtualCameraResourcePtr &camera, const QList<QnPtzPreset> &presets);

    void addPtzPreset(const QnVirtualCameraResourcePtr &camera, const QnPtzPreset &preset);
    void removePtzPreset(const QnVirtualCameraResourcePtr &camera, const QString &name);

private slots:
    void at_context_userChanged();
    void at_presetsLoaded(const QString &value);

    void savePresets();

private:
    QScopedPointer<QnWorkbenchPtzPresetManagerPrivate> d;
};



#endif // QN_WORKBENCH_PTZ_PRESET_MANAGER_H
