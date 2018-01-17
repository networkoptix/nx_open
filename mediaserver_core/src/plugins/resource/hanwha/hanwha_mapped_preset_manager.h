#pragma once

#if defined(ENABLE_HANWHA)

#include <core/resource/resource_fwd.h>
#include <nx/mediaserver/ptz/mapped_preset_manager.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaMappedPresetManager: public nx::mediaserver::ptz::MappedPresetManager
{
    using base_type = nx::mediaserver::ptz::MappedPresetManager;

public:
    HanwhaMappedPresetManager(const QnResourcePtr& resource);
    void setMaxPresetNumber(int maxPresetNumber);

protected:
    virtual bool createNativePreset(const QnPtzPreset& nxPreset, QString *outNativePresetId) override;
    virtual bool removeNativePreset(const QString nativePresetId) override;
    virtual bool nativePresets(QnPtzPresetList* outNativePresetIds) const override;
    virtual bool activateNativePreset(const QString& nativePresetId, qreal speed) override;
    virtual bool normalizeNativePreset(
        const QString& nativePresetId,
        QnPtzPreset* outNativePreset) override;

private:
    QString makePresetId(const QString& number, const QString& name) const;
    QString makeDevicePresetName(const QString& presetNumber) const;
    QString presetNumberFromId(const QString& presetId) const;
    QString presetNameFromId(const QString& presetId) const;

    QString freePresetNumber() const;
    QString channel() const;

private:
    HanwhaResourcePtr m_hanwhaResource;
    int m_maxPresetNumber = 0;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
