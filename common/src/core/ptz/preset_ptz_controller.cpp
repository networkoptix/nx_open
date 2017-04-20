#include "preset_ptz_controller.h"

#include <nx/fusion/model_functions.h>
#include <api/app_server_connection.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

#include "ptz_preset.h"

bool deserialize(const QString& /*value*/, QnPtzPresetRecordHash* /*target*/)
{
    Q_ASSERT_X(0, Q_FUNC_INFO, "Not implemented");
    return false;
}

namespace
{
const QString kPresetsPropertyKey = lit("ptzPresets");
}

// -------------------------------------------------------------------------- //
// Model Data
// -------------------------------------------------------------------------- //

struct QnPtzPresetData
{
    QnPtzPresetData(): space(Qn::DevicePtzCoordinateSpace) {}

    QVector3D position;
    Qn::PtzCoordinateSpace space;
};
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzPresetData, (json)(eq), (position)(space))

struct QnPtzPresetRecord
{
    QnPtzPresetRecord() {}
    QnPtzPresetRecord(const QnPtzPreset &preset, const QnPtzPresetData &data): preset(preset), data(data) {}

    QnPtzPreset preset;
    QnPtzPresetData data;
};
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzPresetRecord, (json)(eq), (preset)(data))

Q_DECLARE_METATYPE(QnPtzPresetRecordHash)


// -------------------------------------------------------------------------- //
// QnPresetPtzController
// -------------------------------------------------------------------------- //
QnPresetPtzController::QnPresetPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_camera(resource().dynamicCast<QnVirtualCameraResource>()),
    m_propertyHandler(new QnJsonResourcePropertyHandler<QnPtzPresetRecordHash>())
{
    NX_ASSERT(!baseController->hasCapabilities(Ptz::Capability::AsynchronousPtzCapability)); // TODO: #Elric
}

QnPresetPtzController::~QnPresetPtzController()
{
    return;
}

bool QnPresetPtzController::extends(Ptz::Capabilities capabilities)
{
    return capabilities.testFlag(Ptz::Capability::AbsolutePtzCapabilities)
        && !capabilities.testFlag(Ptz::Capability::PresetsPtzCapability)
        && (capabilities.testFlag(Ptz::Capability::DevicePositioningPtzCapability)
            || capabilities.testFlag(Ptz::Capability::LogicalPositioningPtzCapability));
}

Ptz::Capabilities QnPresetPtzController::getCapabilities()
{
    /* Note that this controller preserves both Ptz::Capability::AsynchronousPtzCapability and Ptz::Capability::SynchronizedPtzCapability. */
    Ptz::Capabilities capabilities = base_type::getCapabilities();
    return extends(capabilities) ? (capabilities | Ptz::Capability::PresetsPtzCapability) : capabilities;
}

bool QnPresetPtzController::createPreset(const QnPtzPreset &preset)
{
    if (preset.id.isEmpty())
        return false;

    auto createPresetActionFunc = [this](QnPtzPresetRecordHash& records, QnPtzPreset preset)
    {
        QnPtzPresetData data;
        data.space = hasCapabilities(Ptz::Capability::LogicalPositioningPtzCapability) ?
            Qn::LogicalPtzCoordinateSpace :
            Qn::DevicePtzCoordinateSpace;

        if (!getPosition(data.space, &data.position)) // TODO: #Elric this won't work for async base controller.
            return false;

        records.insert(preset.id, QnPtzPresetRecord(preset, data));

        return true;
    };


    bool status = false;
    {
        QnMutexLocker locker(&m_mutex);
        if ((status = doPresetsAction(createPresetActionFunc, preset)))
            m_camera->saveParamsAsync();
    }

    if (status)
        emit changed(Qn::PresetsPtzField);

    return status;
}

bool QnPresetPtzController::updatePreset(const QnPtzPreset &preset)
{
    auto updatePresetActionFunc = [this](QnPtzPresetRecordHash& records, QnPtzPreset preset)
    {
        if (!records.contains(preset.id))
            return false;

        QnPtzPresetRecord &record = records[preset.id];
        if (record.preset == preset)
            return true;

        record.preset = preset;
        return true;
    };


    bool status = false;
    {
        QnMutexLocker locker(&m_mutex);
        if ((status = doPresetsAction(updatePresetActionFunc, preset)))
        {
            NX_ASSERT(m_camera, Q_FUNC_INFO, "Cannot update preset since corresponding resource does not exist.");
            m_camera->saveParamsAsync();
        }
    }

    if (status)
        emit changed(Qn::PresetsPtzField);

    return status;
}

bool QnPresetPtzController::removePreset(const QString &presetId)
{
    auto removePresetActionFunc = [this](QnPtzPresetRecordHash& records, QnPtzPreset preset)
    {
        if (records.remove(preset.id) == 0)
            return false;

        return true;
    };

    bool status = false;
    {
        QnMutexLocker locker(&m_mutex);
        if ((status = doPresetsAction(removePresetActionFunc, QnPtzPreset(presetId, QString()))))
        {
            NX_ASSERT(m_camera, Q_FUNC_INFO, "Cannot remove preset since correspondent resource does not exist.");
            m_camera->saveParamsAsync();
        }
    }

    if (status)
        emit changed(Qn::PresetsPtzField);

    return status;
}

bool QnPresetPtzController::activatePreset(const QString &presetId, qreal speed)
{
    auto activatePresetActionFunc = [this, speed](QnPtzPresetRecordHash& records, QnPtzPreset preset)
    {
        if (!records.contains(preset.id))
            return false;

        QnPtzPresetData data = records.value(preset.id).data;

        if (!absoluteMove(data.space, data.position, speed))
            return false;

        return true;
    };

    return doPresetsAction(activatePresetActionFunc, QnPtzPreset(presetId, QString()));
}

bool QnPresetPtzController::getPresets(QnPtzPresetList *presets)
{
    auto activatePresetActionFunc =
        [this, presets](QnPtzPresetRecordHash& records, QnPtzPreset /*preset*/)
        {
            presets->clear();
            for (const QnPtzPresetRecord &record : records)
                presets->push_back(record.preset);

            return true;
        };

    {
        QnMutexLocker lock(&m_mutex);
        return doPresetsAction(activatePresetActionFunc, QnPtzPreset());
    }

}

QString QnPresetPtzController::serializePresets(const QnPtzPresetRecordHash& presets)
{
    QString serialized;
    m_propertyHandler->serialize(presets, &serialized);

    return serialized;
}

QnPtzPresetRecordHash QnPresetPtzController::deserializePresets(const QString& presetsSerialized)
{
    QnPtzPresetRecordHash deserialized;
    m_propertyHandler->deserialize(presetsSerialized, &deserialized);

    return deserialized;
}

bool QnPresetPtzController::doPresetsAction(PresetsActionFunc actionFunc, QnPtzPreset preset)
{
    if (!m_camera)
        return false;

    auto serialized = m_camera->getProperty(kPresetsPropertyKey);
    auto deserialized = deserializePresets(serialized);

    if (!actionFunc(deserialized, preset))
        return false;

    serialized = serializePresets(deserialized);
    m_camera->setProperty(kPresetsPropertyKey, serialized);

    return true;
}

