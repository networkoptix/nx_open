#include "workbench_ptz_preset_manager.h"

#include <algorithm>

#include <utils/common/warnings.h>
#include <utils/common/json.h>

#include <api/app_server_connection.h>
#include <core/resource/camera_resource.h>

#include "workbench_context.h"

namespace {
    struct PtzPresetData {
        PtzPresetData() {}
        PtzPresetData(const QString &cameraPhysicalId, const QString &name, const QVector3D &logicalPosition): cameraPhysicalId(cameraPhysicalId), name(name), logicalPosition(logicalPosition) {}

        QString cameraPhysicalId;
        QString name;
        QVector3D logicalPosition;
    };

    struct PtzPresetKeyPredicate {
        PtzPresetKeyPredicate(const PtzPresetData &data): data(data) {}

        bool operator()(const PtzPresetData &value) const {
            return value.cameraPhysicalId == data.cameraPhysicalId && value.name == data.name;
        }

        PtzPresetData data;
    };

    inline void serialize(const QVector3D &value, QVariant *target) {
        QVariantMap result;
        QJson::serialize(value.x(), "x", &result);
        QJson::serialize(value.y(), "y", &result);
        QJson::serialize(value.z(), "z", &result);
        *target = result;
    }

    inline bool deserialize(const QVariant &value, QVector3D *target) {
        if(value.type() != QVariant::Map)
            return false;
        QVariantMap map = value.toMap();

        qreal x, y, z;
        if(
            !QJson::deserialize(map, "x", &x) || 
            !QJson::deserialize(map, "y", &y) ||
            !QJson::deserialize(map, "z", &z)
            ) {
                return false;
        }

        *target = QVector3D(x, y, z);
        return true;
    }

    inline void serialize(const PtzPresetData &value, QVariant *target) {
        QVariantMap result;
        QJson::serialize(value.cameraPhysicalId, "cameraPhysicalId", &result);
        QJson::serialize(value.name, "name", &result);
        QJson::serialize(value.logicalPosition, "logicalPosition", &result);
        *target = result;
    }
    
    inline bool deserialize(const QVariant &value, PtzPresetData *target) {
        if(value.type() != QVariant::Map)
            return false;
        QVariantMap map = value.toMap();

        QString cameraPhysicalId, name;
        QVector3D logicalPosition;
        if(
            !QJson::deserialize(map, "cameraPhysicalId", &cameraPhysicalId) || 
            !QJson::deserialize(map, "name", &name) ||
            !QJson::deserialize(map, "logicalPosition", &logicalPosition)
        ) {
                return false;
        }

        *target = PtzPresetData(cameraPhysicalId, name, logicalPosition);
        return true;
    }


} // anonymous namespace


class QnWorkbenchPtzPresetManagerPrivate {
public:
    QnWorkbenchPtzPresetManagerPrivate(): loadHandle(-1), saveHandle(-1) {}

    QVector<PtzPresetData> presets;
    int loadHandle, saveHandle;
};

QnWorkbenchPtzPresetManager::QnWorkbenchPtzPresetManager(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    d(new QnWorkbenchPtzPresetManagerPrivate())
{
    connect(context(), SIGNAL(userChanged(const QnUserResourcePtr &)), this, SLOT(at_context_userChanged()));
}

QnWorkbenchPtzPresetManager::~QnWorkbenchPtzPresetManager() {
    return;
}

QnPtzPreset QnWorkbenchPtzPresetManager::ptzPreset(const QnVirtualCameraResourcePtr &camera, const QString &name) const {
    if(!camera || name.isEmpty())
        return QnPtzPreset();

    QVector<PtzPresetData>::iterator pos = std::find_if(d->presets.begin(), d->presets.end(), PtzPresetKeyPredicate(PtzPresetData(camera->getPhysicalId(), name, QVector3D())));
    if(pos == d->presets.end())
        return QnPtzPreset();

    return QnPtzPreset(pos->name, pos->logicalPosition);
}

QList<QnPtzPreset> QnWorkbenchPtzPresetManager::ptzPresets(const QnVirtualCameraResourcePtr &camera) const {
    if(!camera)
        return QList<QnPtzPreset>();

    QList<QnPtzPreset> result;
    QString physicalId = camera->getPhysicalId();
    foreach(const PtzPresetData &data, d->presets)
        if(data.cameraPhysicalId == physicalId)
            result.push_back(QnPtzPreset(data.name, data.logicalPosition));
    return result;
}

void QnWorkbenchPtzPresetManager::addPtzPreset(const QnVirtualCameraResourcePtr &camera, const QString &name, const QVector3D &logicalPosition) {
    if(!camera) {
        qnNullWarning(camera);
        return;
    }

    if(name.isEmpty()) {
        qnNullWarning(name);
        return;
    }

    PtzPresetData data(camera->getPhysicalId(), name, logicalPosition);
    QVector<PtzPresetData>::iterator pos = std::find_if(d->presets.begin(), d->presets.end(), PtzPresetKeyPredicate(data));
    if(pos != d->presets.end()) {
        *pos = data; /* Replace existing. */
    } else {
        d->presets.push_back(data);
    }

    savePresets();
}

void QnWorkbenchPtzPresetManager::removePtzPreset(const QnVirtualCameraResourcePtr &camera, const QString &name) {
    if(!camera) {
        qnNullWarning(camera);
        return;
    }

    if(name.isEmpty()) {
        qnNullWarning(name);
        return;
    }

    QVector<PtzPresetData>::iterator pos = std::find_if(d->presets.begin(), d->presets.end(), PtzPresetKeyPredicate(PtzPresetData(camera->getPhysicalId(), name, QVector3D())));
    if(pos == d->presets.end())
        return;

    d->presets.erase(pos);

    savePresets();
}

void QnWorkbenchPtzPresetManager::loadPresets() {
    d->loadHandle = QnAppServerConnectionFactory::createConnection()->getKvPairsAsync(this, SLOT(at_connection_replyReceived(int, const QByteArray &, const QnKvPairList &, int)));
}

void QnWorkbenchPtzPresetManager::savePresets() {
    QByteArray data;
    QJson::serialize(d->presets, &data);

    QnKvPairList kvPairs;
    kvPairs.push_back(QnKvPair(lit("ptzPresets"), QString::fromUtf8(data)));

    d->saveHandle = QnAppServerConnectionFactory::createConnection()->saveAsync(kvPairs, this, SLOT(at_connection_replyReceived(int, const QByteArray &, const QnKvPairList &, int)));
}

void QnWorkbenchPtzPresetManager::at_context_userChanged() {
    d->presets.clear();

    if(!context()->user())
        return;

    loadPresets();
}

void QnWorkbenchPtzPresetManager::at_connection_replyReceived(int status, const QByteArray &errorString, const QnKvPairList &kvPairs, int handle) {
    // TODO: check status

    if(status != 0) {
        qnWarning("Failed to save/load ptz presets.");
        return;
    }

    if(handle == d->loadHandle) {
        QByteArray data;
        foreach(const QnKvPair &pair, kvPairs)
            if(pair.name() == lit("ptzPresets"))
                data = pair.value().toUtf8();
        
        if(data.isEmpty()) {
            d->presets.clear();
        } else if(!QJson::deserialize(data, &d->presets)) {
            qnWarning("Invalid ptz preset format.");
        }


        d->loadHandle = -1;
    } else if(handle == d->saveHandle) {
        d->saveHandle = -1;
    }
}


