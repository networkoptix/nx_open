#include "hanwha_mixed_preset_manager.h"
#include "hanwha_resource.h"
#include "hanwha_request_helper.h"

#include <utils/common/app_info.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const int kHanwhaMaxPresetNameLength = 12;

} // namespace

HanwhaMixedPresetManager::HanwhaMixedPresetManager(
    const QnResourcePtr& resource,
    QnAbstractPtzController* controller)
    :
    base_type(resource, controller)
{
    m_hanwhaResource = resource.dynamicCast<HanwhaResource>();
}

bool HanwhaMixedPresetManager::createNativePreset(const QnPtzPreset& nxPreset, QString *outNativePresetId)
{
    HanwhaRequestHelper helper(m_hanwhaResource);
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

bool HanwhaMixedPresetManager::removeNativePreset(const QString nativePresetId)
{
    const auto presetNumber = presetNumberFromId(nativePresetId);
    if (presetNumber.isEmpty())
        return false;

    const auto presetName = presetNameFromId(nativePresetId);

    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto response = helper.remove(
        lit("ptzconfig/preset"),
        {
            {kHanwhaChannelProperty, channel()},
            {kHanwhaPresetNumberProperty, presetNumber},
            {kHanwhaPresetNameProperty, presetName}
        });

    return response.isSuccessful();
}

bool HanwhaMixedPresetManager::nativePresets(QStringList* outNativePresetIds)
{
    NX_EXPECT(outNativePresetIds);
    if (!outNativePresetIds)
        return false;

    QMap<int, QString> presetsOnDevice;
    if (!fetchPresetList(&presetsOnDevice))
        return false;

    for (const auto& presetNumber: presetsOnDevice.keys())
        outNativePresetIds->push_back(presetsOnDevice[presetNumber]);

    return true;
}

bool HanwhaMixedPresetManager::activateNativePreset(const QString& nativePresetId, qreal speed)
{
    HanwhaRequestHelper helper(m_hanwhaResource);

    const auto numberAndName = nativePresetId.split(L':');
    if (numberAndName.size() != 2)
        return false;

    if (!numberAndName[0].isEmpty())
    {
        const auto response = helper.control(
            lit("ptzcontrol/preset"),
            {{kHanwhaPresetNumberProperty, numberAndName[0]}});

        return response.isSuccessful();
    }

    return false;
}

QString HanwhaMixedPresetManager::makeDevicePresetName(const QString& presetNumber) const
{
    NX_EXPECT(!presetNumber.isEmpty());
    if (presetNumber.isEmpty())
        return QString();

    const auto normalizedAppName = QnAppInfo::productNameShort()
        .mid(0, kHanwhaMaxPresetNameLength - presetNumber.length())
        .remove(QRegExp("[^a-zA-Z]"));

    return lit("%1%2")
        .arg(normalizedAppName)
        .arg(presetNumber);
}

QString HanwhaMixedPresetManager::makePresetId(const QString& number, const QString& name) const
{
    return lit("%1:%2").arg(number).arg(name);
}

QString HanwhaMixedPresetManager::freePresetNumber() const
{
    QnPtzPresetList presets;

    if (m_maxPresetNumber <= 0)
        return QString();

    QMap<int, QString> presetsOnDevice;
    if (!fetchPresetList(&presetsOnDevice))
        return QString();

    const int limit = m_maxPresetNumber > 0
        ? m_maxPresetNumber
        : kHanwhaMaxPresetNumber;

    for (auto i = 1; i < limit; ++i)
    {
        if (!presetsOnDevice.contains(i))
            return QString::number(i);
    }

    return QString();
}

QString HanwhaMixedPresetManager::channel() const
{
    return QString::number(m_hanwhaResource->getChannel());
}

QString HanwhaMixedPresetManager::presetNumberFromId(const QString& presetId) const
{
    const auto numberAndName = presetId.split(L':');
    if (numberAndName.size() != 2)
        return QString();

    return numberAndName[0];
}

QString HanwhaMixedPresetManager::presetNameFromId(const QString& presetId) const
{
    const auto numberAndName = presetId.split(L':');
    if (numberAndName.size() != 2)
        return QString();

    return numberAndName[1];
}

void HanwhaMixedPresetManager::setMaxPresetNumber(int maxPresetNumber)
{
    m_maxPresetNumber = maxPresetNumber;
}

bool HanwhaMixedPresetManager::fetchPresetList(QMap<int, QString>* outPresets) const
{
    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto response = helper.view(
        lit("ptzconfig/preset"),
        {{kHanwhaChannelProperty, channel()}});

    if (!response.isSuccessful())
        return false;

    for (const auto& presetEntry : response.response())
    {
        const auto split = presetEntry.first.split(L'.');
        if (split.size() != 5)
            continue;

        bool success = false;
        const auto& presetNumber = split[3].toInt(&success);
        if (!success)
            continue;

        const auto& presetName = makePresetId(
            QString::number(presetNumber),
            presetEntry.second);

        outPresets->insert(presetNumber, presetName);
    }

    return true;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
