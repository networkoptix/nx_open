#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/ptz/mapped_preset_manager.h>
#include <nx/vms/server/resource/resource_fwd.h>

namespace nx {
namespace vms::server {
namespace plugins {

class HanwhaMappedPresetManager: public nx::vms::server::ptz::MappedPresetManager
{
    using base_type = nx::vms::server::ptz::MappedPresetManager;

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
} // namespace vms::server
} // namespace nx
