#if defined(ENABLE_HANWHA)

#include "hanwha_mapped_preset_manager.h"
#include "hanwha_resource.h"
#include "hanwha_request_helper.h"

#include <utils/common/app_info.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const int kHanwhaMaxPresetNameLength = 12;

} // namespace

HanwhaMappedPresetManager::HanwhaMappedPresetManager(const QnResourcePtr& resource):
    base_type(resource)
{
    m_hanwhaResource = resource.dynamicCast<HanwhaResource>();
}

bool HanwhaMappedPresetManager::createNativePreset(const QnPtzPreset& nxPreset, QString *outNativePresetId)
{
    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    const auto presetNumber = freePresetNumber();
    if (presetNumber.isEmpty())
        return false;

    const auto devicePresetName = makeDevicePresetName(presetNumber);

    const auto response = helper.add(
        lit("ptzconfig/preset"),
        {
            {kHanwhaChannelProperty, channel()},
            {kHanwhaPresetNumberProperty, presetNumber},
            {kHanwhaPresetNameProperty, devicePresetName}
        });

    if (!response.isSuccessful())
        return false;

    *outNativePresetId = makePresetId(presetNumber, devicePresetName);
    return true;
}

bool HanwhaMappedPresetManager::removeNativePreset(const QString nativePresetId)
{
    const auto presetNumber = presetNumberFromId(nativePresetId);
    if (presetNumber.isEmpty())
        return false;

    const auto presetName = presetNameFromId(nativePresetId);

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    const auto response = helper.remove(
        lit("ptzconfig/preset"),
        {
            {kHanwhaChannelProperty, channel()},
            {kHanwhaPresetNumberProperty, presetNumber},
            {kHanwhaPresetNameProperty, presetName}
        });

    return response.isSuccessful();
}

bool HanwhaMappedPresetManager::nativePresets(QnPtzPresetList* outNativePresets) const
{
    NX_EXPECT(outNativePresets);
    if (!outNativePresets)
        return false;

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    const auto response = helper.view(
        lit("ptzconfig/preset"),
        {{kHanwhaChannelProperty, channel()}});

    // We can get 602 Configuration not found error if there are no presets for device.
    // It's not an actual error - it just means that the list of presets is empty.
    if (!response.isSuccessful())
        return response.errorCode() == kHanwhaConfigurationNotFoundError;

    for (const auto& presetEntry : response.response())
    {
        const auto split = presetEntry.first.split(L'.');
        if (split.size() != 5)
            continue;

        const auto& presetId = makePresetId(
            split[3] /*prestNumber*/,
            presetEntry.second /*presetName*/);

        outNativePresets->push_back(
            QnPtzPreset(presetId, presetId)); //< Name is the same as id.
    }

    return true;
}

bool HanwhaMappedPresetManager::activateNativePreset(const QString& nativePresetId, qreal speed)
{
    const auto number = presetNumberFromId(nativePresetId);
    if (number.isEmpty())
        return false;

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    const auto response = helper.control(
        lit("ptzcontrol/preset"),
        {
            {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())},
            {kHanwhaPresetNumberProperty, number}
        });

    return response.isSuccessful();
}

bool HanwhaMappedPresetManager::normalizeNativePreset(
    const QString& nativePresetId,
    QnPtzPreset* outNativePreset)
{
    const auto presetNumber = presetNumberFromId(nativePresetId);
    if (presetNumber.isEmpty())
        return false;

    const auto newPresetName = makeDevicePresetName(presetNumber);
    if (newPresetName.isEmpty())
        return false;

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    const auto response = helper.update(
        lit("ptzconfig/preset"),
        {
            {kHanwhaPresetNumberProperty, presetNumber},
            {kHanwhaPresetNameProperty, newPresetName}
        });

    if (!response.isSuccessful())
        return false;

    outNativePreset->id = makePresetId(presetNumber, newPresetName);
    outNativePreset->name = outNativePreset->id;

    return true;
}

QString HanwhaMappedPresetManager::makePresetId(const QString& number, const QString& name) const
{
    return lit("%1:%2").arg(number).arg(name);
}

QString HanwhaMappedPresetManager::makeDevicePresetName(const QString& presetNumber) const
{
    NX_EXPECT(!presetNumber.isEmpty());
    if (presetNumber.isEmpty())
        return QString();

    const auto normalizedAppName = QnAppInfo::productNameLong()
        .mid(0, kHanwhaMaxPresetNameLength - presetNumber.length())
        .remove(QRegExp("[^a-zA-Z]"));

    return lit("%1%2")
        .arg(normalizedAppName)
        .arg(presetNumber);
}

QString HanwhaMappedPresetManager::presetNumberFromId(const QString& presetId) const
{
    const auto numberAndName = presetId.split(L':');
    if (numberAndName.size() != 2)
        return QString();

    return numberAndName[0];
}

QString HanwhaMappedPresetManager::presetNameFromId(const QString& presetId) const
{
    const auto numberAndName = presetId.split(L':');
    if (numberAndName.size() != 2)
        return QString();

    return numberAndName[1];
}

QString HanwhaMappedPresetManager::freePresetNumber() const
{
    if (m_maxPresetNumber <= 0)
        return QString();

    QnPtzPresetList presets;
    if (!nativePresets(&presets))
        return QString();

    QSet<int> presetNumbers;
    for (const auto& preset: presets)
        presetNumbers.insert(presetNumberFromId(preset.id).toInt());

    const int limit = m_maxPresetNumber > 0
        ? m_maxPresetNumber
        : kHanwhaDefaultMaxPresetNumber;

    for (auto i = 1; i < limit; ++i)
    {
        if (!presetNumbers.contains(i))
            return QString::number(i);
    }

    return QString();
}

void HanwhaMappedPresetManager::setMaxPresetNumber(int maxPresetNumber)
{
    m_maxPresetNumber = maxPresetNumber;
}


QString HanwhaMappedPresetManager::channel() const
{
    return QString::number(m_hanwhaResource->getChannel());
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
