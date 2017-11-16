#pragma once

#include <core/ptz/ptz_preset.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace mediaserver {
namespace ptz {

class MappedPresetManager
{

public:
    MappedPresetManager(const QnResourcePtr& resource);
    virtual ~MappedPresetManager() = default;

    bool createPreset(const QnPtzPreset& preset);
    bool updatePreset(const QnPtzPreset& preset);
    bool removePreset(const QString& presetId);
    bool presets(QnPtzPresetList* outPresets);
    bool activatePreset(const QString& preset, qreal speed);

protected:
    virtual bool createNativePreset(const QnPtzPreset& preset, QString *outNativePresetId) = 0;
    virtual bool removeNativePreset(const QString nativePresetId) = 0;
    virtual bool nativePresets(QnPtzPresetList* outNativePreset) const = 0;
    virtual bool activateNativePreset(const QString& nativePresetId, qreal speed) = 0;
    virtual bool normalizeNativePreset(
        const QString& nativePresetId,
        QnPtzPreset* outNativePreset) = 0;

private:
    void loadMappings();
    void createOrUpdateMapping(const QString& devicePresetId, const QnPtzPreset& nxPreset);
    void removeMapping(const QString& nxPresetId, const QString& nativePresetId);
    void applyMapping(const QnPtzPresetList& devicePresets, QnPtzPresetList* outNxPresets) const;

    QString nativePresetId(const QString& nxPresetId) const;

private:
    mutable QnMutex m_mutex;
    QnResourcePtr m_resource;
    mutable QnPtzPresetMapping m_presetMapping;
    QMap<QString, QString> m_nxToNativeId;
};

} // namespace ptz
} // namespace mediaserver
} // namespace nx
