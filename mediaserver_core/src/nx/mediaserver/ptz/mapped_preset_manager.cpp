#include "mapped_preset_manager.h"

#include <core/resource/resource.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>

namespace nx {
namespace mediaserver {
namespace ptz {

namespace {

static const QString kPresetMappingProperty = lit("presetMapping");

} // namespace

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
    QnPtzPreset devicePreset;
    bool result = normalizeNativePreset(preset.id, &devicePreset);
    if (!result)
        return result;

    createOrUpdateMapping(devicePreset.id, preset);    
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
    m_presetMapping = QJson::deserialized<PresetMapping>(
        m_resource->getProperty(kPresetMappingProperty).toUtf8(),
        PresetMapping());
}

void MappedPresetManager::createOrUpdateMapping(const QString& devicePresetId, const QnPtzPreset& nxPreset)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_presetMapping[devicePresetId] = nxPreset.name;
        m_nxToNativeId[nxPreset.id] = devicePresetId;
        const QString serialized = QJson::serialized(m_presetMapping);
        m_resource->setProperty(kPresetMappingProperty, serialized);
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
        m_resource->setProperty(kPresetMappingProperty, serialized);
    }

    m_resource->saveParams();
}

void MappedPresetManager::applyMapping(
    const QnPtzPresetList& devicePresets,
    QnPtzPresetList* outNxPresets) const
{
    QnMutexLocker lock(&m_mutex);
    for (const auto& preset: devicePresets)
    {
        if (m_presetMapping.contains(preset.id))
            outNxPresets->push_back(QnPtzPreset(preset.id, m_presetMapping[preset.id]));
        else
            outNxPresets->push_back(preset);
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
