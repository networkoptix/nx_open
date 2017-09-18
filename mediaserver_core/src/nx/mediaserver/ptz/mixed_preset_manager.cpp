#include "mixed_preset_manager.h"

#include <core/resource/resource.h>
#include <api/resource_property_adaptor.h>
#include <core/ptz/abstract_ptz_controller.h>

namespace nx {
namespace mediaserver {
namespace ptz {

MixedPresetManager::MixedPresetManager(
    const QnResourcePtr& camera,
    QnAbstractPtzController* controller)
    :
    m_camera(camera),
    m_controller(controller)
{
}

bool MixedPresetManager::createPreset(const QnPtzPreset& preset)
{
    QString nativePresetId;
    if (!createNativePreset(preset, &nativePresetId))
        return false;

    QnPtzPreset copy(preset);
    copy.idOnDevice = nativePresetId;
    if (!createNxPreset(copy))
        return false;

    createPresetMapping(preset.id, nativePresetId);
    return true;
}

bool MixedPresetManager::updatePreset(const QnPtzPreset& preset)
{
    if (hasNxPreset(preset.id))
        return updateNxPreset(preset);

    if (hasNativePreset(preset.id))
    {
        QString createdPresetId;
        if (!createNxPreset(preset, /*generate id*/true, &createdPresetId))
            return false;

        createPresetMapping(createdPresetId, preset.id);
    }
    
    return true;
}

bool MixedPresetManager::removePreset(const QString& presetId)
{
    if (hasNxPreset(presetId) && !removeNxPreset(presetId))
        return false;

    auto nativeId = nativePresetIdFromNxId(presetId);
    if (!nativeId.isEmpty())
    {
        if (hasNativePreset(nativeId))
            removeNativePreset(nativeId);
    }

    if (hasNativePreset(presetId))
        removeNativePreset(presetId);

    removePresetMappings(presetId, nativeId);

    return true;
}

bool MixedPresetManager::presets(QList<QnPtzPreset>* presets)
{
    // TODO: #dmishin too complex method fix it if possible.
    NX_EXPECT(presets);
    if (!presets)
        return false;

    // Get presets from database
    QMap<QString, QnPtzPresetRecord> nxPresetRecords;
    bool success = nxPresets(&nxPresetRecords);
    if (!success)
        return false;

    QSet<QString> nxPresetIds;
    QMap<QString, QString> nativeToNxPresetIds;
    for (const auto& nxId: nxPresetRecords.keys())
    {
        const auto& nxPreset = nxPresetRecords[nxId].preset;
        const auto nativeId = nxPreset.idOnDevice;

        if (!nativeId.isEmpty())
            nativeToNxPresetIds[nativeId] = nxPreset.id;

        nxPresetIds.insert(nxId);
        presets->push_back(nxPreset);
    }

    // Get native presets from the device.
    QStringList nativeResult;
    success = nativePresets(&nativeResult);
    if (!success)
        return false;

    QSet<QString> nativePresets;
    QMap<QString, QString> nxToNativePresetIds;
    for (const auto& nativePresetId: nativeResult)
    {
        nativePresets.insert(nativePresetId);
        // If preset is not mapped - add it to the list else just insert it to the mapping
        if (!nativeToNxPresetIds.contains(nativePresetId))
            presets->push_back(QnPtzPreset(nativePresetId, nativePresetId, nativePresetId));
        else 
            nxToNativePresetIds[nativeToNxPresetIds[nativePresetId]] = nativePresetId;
    }

    // Calculate obsolete mapped presets those doesn't exist on the device anymore.
    QSet<QString> obsoleteMappedPresets;
    for (const auto& mappedPresetId: nativeToNxPresetIds.keys())
    {
        if (!nativePresets.contains(mappedPresetId))
        {
            obsoleteMappedPresets.insert(mappedPresetId);
            nativeToNxPresetIds.remove(mappedPresetId);
        }
    }

    // Correct output preset list according to the obsolete mapped preset list.
    for (auto& preset: *presets)
    {
        if (obsoleteMappedPresets.contains(preset.idOnDevice))
        {
            preset.idOnDevice = QString();
            nxToNativePresetIds.remove(preset.id);
        }
    }

    // Remove obsolete mappings from the database.
    QSet<QString> removedNxPresets;
    removeObsoleteMappings(obsoleteMappedPresets, &removedNxPresets);

    // Filter output presets list (remove obsolete nx presets).
    for (auto itr = presets->begin(); itr != presets->end();)
    {
        if (removedNxPresets.contains(itr->id))
            itr = presets->erase(itr);
        else
            ++itr;
    }

    {
        QnMutexLocker lock(&m_mutex);
        m_nxPresets.swap(nxPresetIds);
        m_nativePresets.swap(nativePresets);
        m_nxToDeviceMapping.swap(nxToNativePresetIds);
        m_deviceToNxMapping.swap(nativeToNxPresetIds);
    }

    return true;
}

bool MixedPresetManager::activatePreset(const QString& presetId, qreal speed)
{
    bool success = false;
    if (hasNativePreset(presetId))
        success = activateNativePreset(presetId, speed);

    if (success)
        return true;

    const auto mappedNativePreset = nativePresetIdFromNxId(presetId);
    if (!mappedNativePreset.isEmpty())
        success = activateNativePreset(mappedNativePreset, speed);

    if (success)
        return true;

    if (hasNxPreset(presetId))
        success = activateNxPreset(presetId, speed);

    return success;
}

bool MixedPresetManager::createNxPreset(
    const QnPtzPreset& preset,
    bool generateId,
    QString* outNxPresetId)
{
    auto createPresetFunc =
        [this, generateId, outNxPresetId](
            QnPtzPresetRecordHash& records,
            const QnPtzPreset& preset)
        {
            QnPtzPresetRecord record;

            // If generateId is true then we're creating nx preset for existing device preset
            record.preset.id = generateId ? QnUuid::createUuid().toString() : preset.id;
            record.preset.name = preset.name;
            record.preset.idOnDevice = generateId ? preset.id : preset.idOnDevice;

            // We can get position only if we're creating new preset but can not 
            // when wrapping existing device preset into NX one.
            Qn::PtzCoordinateSpace space = Qn::InvalidPtzCoordinateSpace;
            if (!generateId)
            {
                space = m_controller->hasCapabilities(Ptz::LogicalPositioningPtzCapability)
                    ? Qn::LogicalPtzCoordinateSpace
                    : Qn::DevicePtzCoordinateSpace;
            }
            
            record.data.space = space;
            
            QVector3D position;
            if (!generateId)
            {
                bool success = m_controller->getPosition(space, &position);
                if (!success)
                    return false;
            }

            record.data.position = position;
            records.insert(record.preset.id, record);

            if (outNxPresetId)
                *outNxPresetId = preset.id;

            return true;
        };

    return doPresetsAction(createPresetFunc, preset);
}

bool MixedPresetManager::updateNxPreset(const QnPtzPreset& preset)
{
    auto updatePresetFunc =
        [this](QnPtzPresetRecordHash& records, const QnPtzPreset& preset)
        {
            if (!records.contains(preset.id))
                return false;

            records[preset.id].preset.name = preset.name;
            return true;
        };

    return doPresetsAction(updatePresetFunc, preset);
}

bool MixedPresetManager::removeNxPreset(const QString& presetId)
{
    auto removePresetFunc =
        [this, &presetId](QnPtzPresetRecordHash& records, const QnPtzPreset& preset)
        {
            records.remove(presetId);
            return true;
        };

    return doPresetsAction(removePresetFunc);
}

bool MixedPresetManager::nxPresets(QMap<QString, QnPtzPresetRecord>* outNxPresets)
{
    NX_EXPECT(outNxPresets);
    if (!outNxPresets)
        return false;

    auto getPresetsFunc = 
        [this, &outNxPresets](QnPtzPresetRecordHash& records, const QnPtzPreset& preset)
        {
            for (const auto& key : records.keys())
            {
                QString nxPresetId = records[key].preset.id;
                outNxPresets->insert(nxPresetId, records[nxPresetId]);
            }

            return true;
        };
    
    return doPresetsAction(getPresetsFunc);
}

bool MixedPresetManager::activateNxPreset(const QString& presetId, qreal speed)
{
    const bool hasAbsoluteMove = (m_controller->getCapabilities() & Ptz::AbsolutePtzCapabilities)
        == Ptz::AbsolutePtzCapabilities;

    if (!hasAbsoluteMove)
        return false;

    auto activatePresetActionFunc = [this, speed](QnPtzPresetRecordHash& records, QnPtzPreset preset)
    {
        if (!records.contains(preset.id))
            return false;

        QnPtzPresetData data = records.value(preset.id).data;

        if (!m_controller->absoluteMove(data.space, data.position, speed))
            return false;

        return true;
    };

    QnPtzPreset preset(presetId, QString());
    return doPresetsAction(activatePresetActionFunc, preset);
}

void MixedPresetManager::createPresetMapping(const QString& nxId, const QString& nativeId)
{
    QnMutexLocker lock(&m_mutex);
    m_nxToDeviceMapping[nxId] = nativeId;
    m_deviceToNxMapping[nativeId] = nxId;
    m_nativePresets.insert(nativeId);
    m_nxPresets.insert(nxId);
    
}

void MixedPresetManager::removePresetMappings(const QString& nxId, const QString& nativeId)
{
    QnMutexLocker lock(&m_mutex);
    m_nxToDeviceMapping.remove(nxId);
    m_deviceToNxMapping.remove(nativeId);
    m_nxPresets.remove(nxId);
    m_nativePresets.remove(nativeId);
}

bool MixedPresetManager::hasNxPreset(const QString& nxId) const
{
    return m_nxPresets.contains(nxId);
}

bool MixedPresetManager::hasNativePreset(const QString& nativeId) const
{
    return m_nativePresets.contains(nativeId);
}

QString MixedPresetManager::nativePresetIdFromNxId(const QString& nxId) const
{
    QnMutexLocker lock(&m_mutex);
    if (m_nxToDeviceMapping.contains(nxId))
        return m_nxToDeviceMapping[nxId];

    return QString();
}

QString MixedPresetManager::serializePresets(const QnPtzPresetRecordHash& presets)
{
    QString serialized;
    QnJsonResourcePropertyHandler<QnPtzPresetRecordHash> handler;
    handler.serialize(presets, &serialized);

    return serialized;
}

QnPtzPresetRecordHash MixedPresetManager::deserializePresets(const QString& presetsSerialized)
{
    QnPtzPresetRecordHash deserialized;
    QnJsonResourcePropertyHandler<QnPtzPresetRecordHash> handler;
    handler.deserialize(presetsSerialized, &deserialized);

    return deserialized;
}

bool MixedPresetManager::doPresetsAction(PresetsActionFunc actionFunc, const QnPtzPreset& preset)
{
    auto serialized = m_camera->getProperty(kPresetsPropertyKey);
    auto deserialized = deserializePresets(serialized);

    if (!actionFunc(deserialized, preset))
        return false;

    serialized = serializePresets(deserialized);
    m_camera->setProperty(kPresetsPropertyKey, serialized);
    m_camera->saveParams();

    return true;
}

bool MixedPresetManager::removeObsoleteMappings(
    const QSet<QString>& obsoleteNativeIds,
    QSet<QString>* outRemovedNxPresets)
{
    NX_EXPECT(outRemovedNxPresets);
    if (!outRemovedNxPresets)
        return false;

    auto removeObsoleteMappingsFunc = 
        [&obsoleteNativeIds, outRemovedNxPresets](
            QnPtzPresetRecordHash& records,
            const QnPtzPreset& /*preset*/)
        {
            for (const auto& presetId: records.keys())
            {
                auto& preset = records[presetId].preset;
                const auto& data = records[presetId].data;
                const auto nativeId = preset.idOnDevice; 

                if (nativeId.isEmpty())
                    continue;

                if (obsoleteNativeIds.contains(nativeId))
                {
                    if (data.space == Qn::InvalidPtzCoordinateSpace)
                    {
                        records.remove(presetId);
                        outRemovedNxPresets->insert(presetId);
                    }
                    else
                        preset.idOnDevice = QString();
                }
            }

            records.remove(QString());

            return true;
        };

    return doPresetsAction(removeObsoleteMappingsFunc);
}

} // namespace ptz
} // namespace mediaserver
} // namespace nx
