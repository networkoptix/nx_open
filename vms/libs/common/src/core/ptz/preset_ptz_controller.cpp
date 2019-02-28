#include "preset_ptz_controller.h"
#include "ptz_preset.h"

#include <nx/fusion/model_functions.h>
#include <api/app_server_connection.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

using namespace nx::core;

bool deserialize(const QString& /*value*/, QnPtzPresetRecordHash* /*target*/)
{
    NX_ASSERT(false, "Not implemented");
    return false;
}

// -------------------------------------------------------------------------- //
// QnPresetPtzController
// -------------------------------------------------------------------------- //
QnPresetPtzController::QnPresetPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_camera(resource().dynamicCast<QnVirtualCameraResource>()),
    m_propertyHandler(new QnJsonResourcePropertyHandler<QnPtzPresetRecordHash>())
{
    NX_ASSERT(!baseController->hasCapabilities(Ptz::AsynchronousPtzCapability));
}

QnPresetPtzController::~QnPresetPtzController()
{
    return;
}

bool QnPresetPtzController::extends(Ptz::Capabilities capabilities, bool preferSystemPresets)
{
    return (capabilities & Ptz::AbsolutePtzCapabilities)
        && (preferSystemPresets || !capabilities.testFlag(Ptz::PresetsPtzCapability));
}

Ptz::Capabilities QnPresetPtzController::getCapabilities(
    const nx::core::ptz::Options& options) const
{
    // Note that this controller preserves both Ptz::AsynchronousPtzCapability
    // and Ptz::SynchronizedPtzCapability.
    Ptz::Capabilities capabilities = base_type::getCapabilities(options);
    if (options.type != ptz::Type::operational)
        return capabilities;

    return extends(capabilities) ? (capabilities | Ptz::PresetsPtzCapability) : capabilities;
}

bool QnPresetPtzController::createPreset(const QnPtzPreset &preset)
{
    if (preset.id.isEmpty())
        return false;

    auto createPresetActionFunc =
        [this](QnPtzPresetRecordHash& records, QnPtzPreset preset)
        {
            QnPtzPresetData data;
            data.space = hasCapabilities(Ptz::LogicalPositioningPtzCapability)
                ? Qn::LogicalPtzCoordinateSpace
                : Qn::DevicePtzCoordinateSpace;

            // TODO: #Elric this won't work for async base controller.
            if (!getPosition(data.space, &data.position, {nx::core::ptz::Type::operational}))
                return false;

            records.insert(preset.id, QnPtzPresetRecord(preset, data));

            return true;
        };

    bool status = false;
    {
        QnMutexLocker locker(&m_mutex);
        if ((status = doPresetsAction(createPresetActionFunc, preset)))
            m_camera->savePropertiesAsync();
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
            NX_ASSERT(m_camera, "Cannot update preset since corresponding resource does not exist.");
            m_camera->savePropertiesAsync();
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
            NX_ASSERT(m_camera, "Cannot remove preset since correspondent resource does not exist.");
            m_camera->savePropertiesAsync();
        }
    }

    if (status)
        emit changed(Qn::PresetsPtzField);

    return status;
}

bool QnPresetPtzController::activatePreset(const QString &presetId, qreal speed)
{
    auto activatePresetActionFunc =
        [this, speed](QnPtzPresetRecordHash& records, QnPtzPreset preset)
        {
            if (!records.contains(preset.id))
                return false;

            QnPtzPresetData data = records.value(preset.id).data;

            if (!absoluteMove(data.space, data.position, speed, {nx::core::ptz::Type::operational}))
                return false;

            return true;
        };

    return doPresetsAction(activatePresetActionFunc, QnPtzPreset(presetId, QString()));
}

bool QnPresetPtzController::getPresets(QnPtzPresetList *presets) const
{
    NX_ASSERT(presets);
    auto getPresetActionFunc =
        [this, presets](QnPtzPresetRecordHash& records, QnPtzPreset /*preset*/)
        {
            presets->clear();
            for (const QnPtzPresetRecord &record : records)
                presets->push_back(record.preset);

            return true;
        };

    {
        QnMutexLocker lock(&m_mutex);
        const auto nonConstThis = const_cast<QnPresetPtzController*>(this);
        return nonConstThis->doPresetsAction(getPresetActionFunc, QnPtzPreset());
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
