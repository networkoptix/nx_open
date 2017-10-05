#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/mediaserver/ptz/mixed_preset_manager.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaMixedPresetManager: public nx::mediaserver::ptz::MixedPresetManager
{
    using base_type = nx::mediaserver::ptz::MixedPresetManager;

public:
    HanwhaMixedPresetManager(
        const QnResourcePtr& resource,
        QnAbstractPtzController* controller);

    void setMaxPresetNumber(int maxPresetNumber);

protected:
    virtual bool createNativePreset(const QnPtzPreset& nxPreset, QString *outNativePresetId) override;
    virtual bool removeNativePreset(const QString nativePresetId) override;
    virtual bool nativePresets(QStringList* outNativePresetIds) override;
    virtual bool activateNativePreset(const QString& nativePresetId, qreal speed) override;

private:
    QString makeDevicePresetName(const QString& presetNumber) const;
    QString makePresetId(const QString& number, const QString& name) const;
    bool fetchPresetList(QMap<int, QString>* outPresets) const;
    QString freePresetNumber() const;
    QString channel() const;
    QString presetNumberFromId(const QString& presetId) const;
    QString presetNameFromId(const QString& presetId) const;

private:
    HanwhaResourcePtr m_hanwhaResource;
    int m_maxPresetNumber = 0;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
