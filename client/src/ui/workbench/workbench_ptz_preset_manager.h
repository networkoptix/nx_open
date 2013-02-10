#ifndef QN_WORKBENCH_PTZ_PRESET_MANAGER_H
#define QN_WORKBENCH_PTZ_PRESET_MANAGER_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <api/model/kvpair.h>
#include <core/resource/resource_fwd.h>

#include "workbench_context_aware.h"

struct QnPtzPreset {
    QnPtzPreset() {};
    QnPtzPreset(const QString &name, const QVector3D &logicalPosition): name(name), logicalPosition(logicalPosition) {}

    bool isNull() const {
        return name.isEmpty();
    }

    QString name;
    QVector3D logicalPosition;
};


class QnWorkbenchPtzPresetManagerPrivate;

class QnWorkbenchPtzPresetManager: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchPtzPresetManager(QObject *parent = NULL);
    virtual ~QnWorkbenchPtzPresetManager();

    QnPtzPreset ptzPreset(const QnVirtualCameraResourcePtr &camera, const QString &name) const;
    QList<QnPtzPreset> ptzPresets(const QnVirtualCameraResourcePtr &camera) const;
    void setPtzPresets(const QnVirtualCameraResourcePtr &camera, const QList<QnPtzPreset> &presets);

    void addPtzPreset(const QnVirtualCameraResourcePtr &camera, const QString &name, const QVector3D &logicalPosition);
    void removePtzPreset(const QnVirtualCameraResourcePtr &camera, const QString &name);

private slots:
    void at_context_userChanged();
    void at_connection_replyReceived(int status, const QByteArray &errorString, const QnKvPairList &kvPairs, int handle);

    void loadPresets();
    void savePresets();

private:
    QScopedPointer<QnWorkbenchPtzPresetManagerPrivate> d;
};



#endif // QN_WORKBENCH_PTZ_PRESET_MANAGER_H
