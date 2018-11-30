#include "mapped_preset_manager.h"

#include <core/resource/resource.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace vms::server {
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

        NX_DEBUG(this, lm("Loaded device preset %1 - %2")
            .args(devicePresetId,  QJson::serialized(m_presetMapping[devicePresetId])));
    }
}

void MappedPresetManager::createOrUpdateMapping(
    const QString& devicePresetId,
    const QnPtzPreset& nxPreset)
{
    NX_DEBUG(this, lm("Update device preset %1 - %2")
        .args(devicePresetId,  QJson::serialized(nxPreset)));

    {
        QnMutexLocker lock(&m_mutex);
        m_presetMapping[devicePresetId] = nxPreset;
        m_nxToNativeId[nxPreset.id] = devicePresetId;
        const QString serialized = QJson::serialized(m_presetMapping);
        m_resource->setProperty(kPtzPresetMappingPropertyName, serialized);
    }

    m_resource->saveProperties();
}

void MappedPresetManager::removeMapping(const QString& nxPresetId, const QString& nativePresetId)
{
    NX_DEBUG(this, lm("Remove device preset %1 - %2")
        .args(nativePresetId,  QJson::serialized(nxPresetId)));

    {
        QnMutexLocker lock(&m_mutex);
        m_presetMapping.remove(nativePresetId);
        m_nxToNativeId.remove(nxPresetId);

        const QString serialized = QJson::serialized(m_presetMapping);
        m_resource->setProperty(kPtzPresetMappingPropertyName, serialized);
    }

    m_resource->saveProperties();
}

void MappedPresetManager::applyMapping(
    const QnPtzPresetList& devicePresets,
    QnPtzPresetList* outNxPresets) const
{
    QnMutexLocker lock(&m_mutex);
    QSet<QString> currentDevicePresets;
    for (auto devicePreset: devicePresets)
    {
        currentDevicePresets.insert(devicePreset.id);
        if (!m_presetMapping.contains(devicePreset.id))
            NX_VERBOSE(this, lm("Camera only preset %1").arg(QJson::serialized(devicePreset)));

        const auto nxPreset = m_presetMapping.value(devicePreset.id);
        if (!nxPreset.id.isEmpty())
            devicePreset.id = nxPreset.id;

        if (!nxPreset.name.isEmpty())
            devicePreset.name = nxPreset.name;

        outNxPresets->push_back(devicePreset);
    }

    bool hasChanges = false;
    for (auto itr = m_presetMapping.begin(); itr != m_presetMapping.end();)
    {
        if (!currentDevicePresets.contains(itr.key()))
        {
            NX_DEBUG(this, lm("Remove device preset %1 during mapping apply").arg(itr.key()));
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
        m_resource->saveProperties();
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
} // namespace vms::server
} // namespace nx
