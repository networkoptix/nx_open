#include "workbench_ptz_preset_manager.h"

#include <iterator> /* For std::back_inserter. */

#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm_ext/erase.hpp>

#include <utils/common/warnings.h>
#include <utils/common/json.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <utils/kvpair_usage_helper.h>

#include "workbench_context.h"

namespace {
    struct PtzPresetData {
        PtzPresetData(): hotkey(-1) {}
        PtzPresetData(const QString &cameraPhysicalId, int hotkey, const QString &name, const QVector3D &logicalPosition): 
            cameraPhysicalId(cameraPhysicalId), hotkey(hotkey), name(name), logicalPosition(logicalPosition) {}

        QnPtzPreset preset() const {
            return QnPtzPreset(hotkey, name, logicalPosition);
        }

        QString cameraPhysicalId;
        int hotkey;
        QString name;
        QVector3D logicalPosition;
    };

    struct PtzPresetNamePredicate {
        PtzPresetNamePredicate(const PtzPresetData &data): data(data) {}

        bool operator()(const PtzPresetData &value) const {
            return value.cameraPhysicalId == data.cameraPhysicalId && value.name == data.name;
        }

        PtzPresetData data;
    };

    struct PtzPresetHotkeyPredicate {
        PtzPresetHotkeyPredicate(const PtzPresetData &data): data(data) {}

        bool operator()(const PtzPresetData &value) const {
            return value.cameraPhysicalId == data.cameraPhysicalId && value.hotkey == data.hotkey;
        }

        PtzPresetData data;
    };

    struct PtzPresetCameraPredicate {
        PtzPresetCameraPredicate(const QString &cameraPhysicaId): cameraPhysicaId(cameraPhysicaId) {}

        bool operator()(const PtzPresetData &value) const {
            return value.cameraPhysicalId == cameraPhysicaId;
        }
    
        QString cameraPhysicaId;
    };

} // anonymous namespace


// TODO: #Elric move out
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
    if(value.hotkey != -1)
        QJson::serialize(value.hotkey, "hotkey", &result);
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
    
    /* Optional field. */
    int hotkey = -1;
    QJson::deserialize(map, "hotkey", &hotkey);

    *target = PtzPresetData(cameraPhysicalId, hotkey, name, logicalPosition);
    return true;
}


class QnWorkbenchPtzPresetManagerPrivate {
public:
    QnWorkbenchPtzPresetManagerPrivate() {}

    QVector<PtzPresetData> presets;
    QnStringKvPairUsageHelper* helper;
};

QnWorkbenchPtzPresetManager::QnWorkbenchPtzPresetManager(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    d(new QnWorkbenchPtzPresetManagerPrivate())
{
      //TODO: #Elric provide correct resource
    connect(context(), SIGNAL(userChanged(const QnUserResourcePtr &)), this, SLOT(at_context_userChanged()));
    d->helper = new QnStringKvPairUsageHelper(context()->user(), lit("ptzPresets"), QString(), this);
    connect(d->helper, SIGNAL(valueChanged(QString)), this, SLOT(at_presetsLoaded(QString)));
}

QnWorkbenchPtzPresetManager::~QnWorkbenchPtzPresetManager() {
    return;
}

QnPtzPreset QnWorkbenchPtzPresetManager::ptzPreset(const QnVirtualCameraResourcePtr &camera, const QString &name) const {
    if(!camera || name.isEmpty())
        return QnPtzPreset();

    QVector<PtzPresetData>::iterator pos = boost::find_if(d->presets, PtzPresetNamePredicate(PtzPresetData(camera->getPhysicalId(), -1, name, QVector3D())));
    if(pos == d->presets.end())
        return QnPtzPreset();

    return pos->preset();
}

QnPtzPreset QnWorkbenchPtzPresetManager::ptzPreset(const QnVirtualCameraResourcePtr &camera, int hotkey) const {
    if(!camera || hotkey < 0)
        return QnPtzPreset();

    QVector<PtzPresetData>::iterator pos = boost::find_if(d->presets, PtzPresetHotkeyPredicate(PtzPresetData(camera->getPhysicalId(), hotkey, QString(), QVector3D())));
    if(pos == d->presets.end())
        return QnPtzPreset();

    return pos->preset();
}

QList<QnPtzPreset> QnWorkbenchPtzPresetManager::ptzPresets(const QnVirtualCameraResourcePtr &camera) const {
    if(!camera)
        return QList<QnPtzPreset>();

    QList<QnPtzPreset> result;
    QString physicalId = camera->getPhysicalId();
    foreach(const PtzPresetData &data, d->presets)
        if(data.cameraPhysicalId == physicalId)
            result.push_back(data.preset());
    return result;
}

void QnWorkbenchPtzPresetManager::setPtzPresets(const QnVirtualCameraResourcePtr &camera, const QList<QnPtzPreset> &presets) {
    if(!camera) {
        qnNullWarning(camera);
        return;
    }

    boost::remove_erase_if(d->presets, PtzPresetCameraPredicate(camera->getPhysicalId()));
    
    QString cameraPhysicalId = camera->getPhysicalId();
    foreach(const QnPtzPreset &preset, presets)
        d->presets.push_back(PtzPresetData(cameraPhysicalId, preset.hotkey, preset.name, preset.logicalPosition));

    savePresets();
}

void QnWorkbenchPtzPresetManager::addPtzPreset(const QnVirtualCameraResourcePtr &camera, int hotkey, const QString &name, const QVector3D &logicalPosition) {
    if(!camera) {
        qnNullWarning(camera);
        return;
    }

    if(name.isEmpty()) {
        qnNullWarning(name);
        return;
    }

    PtzPresetData data(camera->getPhysicalId(), hotkey, name, logicalPosition);
    QVector<PtzPresetData>::iterator pos = boost::find_if(d->presets, PtzPresetNamePredicate(data));
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

    QVector<PtzPresetData>::iterator pos = boost::find_if(d->presets, PtzPresetNamePredicate(PtzPresetData(camera->getPhysicalId(), -1, name, QVector3D())));
    if(pos == d->presets.end())
        return;

    d->presets.erase(pos);

    savePresets();
}

void QnWorkbenchPtzPresetManager::savePresets() {
    QByteArray data;
    QJson::serialize(d->presets, &data);
    d->helper->setValue(QString::fromUtf8(data));
}

void QnWorkbenchPtzPresetManager::at_context_userChanged() {
    //TODO: #Elric provide correct resource
    d->presets.clear();
    d->helper->setResource(context()->user());
}

void QnWorkbenchPtzPresetManager::at_presetsLoaded(const QString &value) {
    QByteArray data = value.toUtf8();
    if(data.isEmpty()) {
        d->presets.clear();
    } else if(!QJson::deserialize(data, &d->presets)) {
        qnWarning("Invalid ptz preset format.");
    }
}
