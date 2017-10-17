#include "mapped_preset_manager.h"

#include <core/resource/resource.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>

namespace nx {
namespace mediaserver {
namespace ptz {

MappedPresetManager::MappedPresetManager(const QnResourcePtr& resource):
    m_resource(resource)
{
    loadMappings();
}

bool MappedPresetManager::createPreset(const QnPtzPreset& preset)
{
    QString nativePresetId;
    if (!createNativePreset(preset, &nativePresetId))
        return false;

    createOrUpdateMapping(nativePresetId, preset);
    return true;
}

bool MappedPresetManager::updatePreset(const QnPtzPreset& preset)
{
    QString devicePresetId;
    {
        QnMutexLocker lock(&m_mutex);
        devicePresetId = m_nxToNativeId.value(preset.id);
    }

    if (devicePresetId.isEmpty())
        devicePresetId = preset.id;

    createOrUpdateMapping(devicePresetId, preset);    
    return true;
}

bool MappedPresetManager::removePreset(const QString& nxPresetId)
{
    const QString nativeId = nativePresetId(nxPresetId);
    bool result = removeNativePreset(nativeId);
    if (!result)
        return false;

    removeMapping(nxPresetId, nativeId);
    return true;
}

bool MappedPresetManager::presets(QnPtzPresetList* outPresets)
{
    QnPtzPresetList presetsFromDevice;
    bool result = nativePresets(&presetsFromDevice);

    if (!result)
        return false;

    applyMapping(presetsFromDevice, outPresets);
    return true;
}

bool MappedPresetManager::activatePreset(const QString& nxPresetId, qreal speed)
{
    return activateNativePreset(nativePresetId(nxPresetId), speed);
}

void MappedPresetManager::loadMappings()
{
    QnMutexLocker lock(&m_mutex);
    m_presetMapping = QJson::deserialized<QnPtzPresetMapping>(
        m_resource->getProperty(kPtzPresetMappingPropertyName).toUtf8(),
        QnPtzPresetMapping());

    m_nxToNativeId.clear();
    for (const auto devicePresetId: m_presetMapping.keys())
    {
        const auto nxPresetId = m_presetMapping[devicePresetId].id;
        m_nxToNativeId[nxPresetId] = devicePresetId;
    }
}

void MappedPresetManager::createOrUpdateMapping(const QString& devicePresetId, const QnPtzPreset& nxPreset)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_presetMapping[devicePresetId] = nxPreset;
        m_nxToNativeId[nxPreset.id] = devicePresetId;
        const QString serialized = QJson::serialized(m_presetMapping);
        m_resource->setProperty(kPtzPresetMappingPropertyName, serialized);
    }

    m_resource->saveParams();
}

void MappedPresetManager::removeMapping(const QString& nxPresetId, const QString& nativePresetId)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_presetMapping.remove(nativePresetId);
        m_nxToNativeId.remove(nxPresetId);

        const QString serialized = QJson::serialized(m_presetMapping);
        m_resource->setProperty(kPtzPresetMappingPropertyName, serialized);
    }

    m_resource->saveParams();
}

void MappedPresetManager::applyMapping(
    const QnPtzPresetList& devicePresets,
    QnPtzPresetList* outNxPresets) const
{
    QnMutexLocker lock(&m_mutex);
    QSet<QString> currentDevicePresets;
    for (const auto& devicePreset: devicePresets)
    {
        currentDevicePresets.insert(devicePreset.id);
        if (m_presetMapping.contains(devicePreset.id))
        {
            auto presetId = m_presetMapping[devicePreset.id].id;
            if (presetId.isEmpty())
                presetId = devicePreset.id;

            outNxPresets->push_back(
                QnPtzPreset(presetId, m_presetMapping[devicePreset.id].name));
        }
        else
        {
            outNxPresets->push_back(devicePreset);
        }
    }

    bool hasChanges = false;
    for (auto itr = m_presetMapping.begin(); itr != m_presetMapping.end();)
    {
        if (!currentDevicePresets.contains(itr.key()))
        {
            itr = m_presetMapping.erase(itr);
            hasChanges = true;
        }
        else
        {
            ++itr;
        }
    }

    if (hasChanges)
    {
        const QString serialized = QJson::serialized(m_presetMapping);
        m_resource->setProperty(kPtzPresetMappingPropertyName, serialized);
        m_resource->saveParams();
    }
}

QString MappedPresetManager::nativePresetId(const QString& nxPresetId) const
{
    QnMutexLocker lock(&m_mutex);
    return m_nxToNativeId.contains(nxPresetId)
        ? m_nxToNativeId[nxPresetId]
        : nxPresetId;
}

} // namespace ptz
} // namespace mediaserver
} // namespace nx
