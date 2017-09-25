#include "preset_ptz_controller.h"
#include "ptz_preset.h"

#include <nx/fusion/model_functions.h>
#include <api/app_server_connection.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

bool deserialize(const QString& /*value*/, QnPtzPresetRecordHash* /*target*/)
{
    Q_ASSERT_X(0, Q_FUNC_INFO, "Not implemented");
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
    NX_ASSERT(!baseController->hasCapabilities(Ptz::AsynchronousPtzCapability)); // TODO: #Elric
}

QnPresetPtzController::~QnPresetPtzController()
{
    return;
}

bool QnPresetPtzController::extends(Ptz::Capabilities capabilities, bool disableNative)
{
    return (capabilities & Ptz::AbsolutePtzCapabilities) == Ptz::AbsolutePtzCapabilities
        && (disableNative || !capabilities.testFlag(Ptz::PresetsPtzCapability))
        && (capabilities.testFlag(Ptz::DevicePositioningPtzCapability)
            || capabilities.testFlag(Ptz::LogicalPositioningPtzCapability));
}

Ptz::Capabilities QnPresetPtzController::getCapabilities() const
{
    /* Note that this controller preserves both Ptz::AsynchronousPtzCapability and Ptz::SynchronizedPtzCapability. */
    Ptz::Capabilities capabilities = base_type::getCapabilities();
    return extends(capabilities) ? (capabilities | Ptz::PresetsPtzCapability) : capabilities;
}

bool QnPresetPtzController::createPreset(const QnPtzPreset &preset)
{
    if (preset.id.isEmpty())
        return false;

    auto createPresetActionFunc = [this](QnPtzPresetRecordHash& records, QnPtzPreset preset)
    {
        QnPtzPresetData data;
        data.space = hasCapabilities(Ptz::LogicalPositioningPtzCapability) ?
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

bool QnPresetPtzController::getPresets(QnPtzPresetList *presets) const
{
    NX_EXPECT(presets);
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

