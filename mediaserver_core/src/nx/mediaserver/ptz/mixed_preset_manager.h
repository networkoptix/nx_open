#pragma once

#include <QtCore/QSet>
#include <QtCore/QMap>

#include <core/ptz/ptz_preset.h>
#include <core/resource/resource_fwd.h>
#include <common/common_globals.h>
#include <nx/utils/thread/mutex.h>
#include <core/ptz/abstract_ptz_controller.h>

namespace nx {
namespace mediaserver {
namespace ptz {

using PresetsActionFunc = std::function<bool(QnPtzPresetRecordHash& records, const QnPtzPreset& preset)>;

class MixedPresetManager
{
public:
    explicit MixedPresetManager(const QnResourcePtr& camera, QnAbstractPtzController* controller);
    virtual ~MixedPresetManager() = default;
    bool createPreset(const QnPtzPreset& preset);
    bool updatePreset(const QnPtzPreset& preset);
    bool removePreset(const QString& presetId);
    bool presets(QList<QnPtzPreset>* outPresets);
    bool activatePreset(const QString& preset, qreal speed);

protected:
    virtual bool createNativePreset(const QnPtzPreset& nxPreset, QString *outNativePresetId) = 0;
    virtual bool removeNativePreset(const QString nativePresetId) = 0;
    virtual bool nativePresets(QStringList* outNativePresetIds) = 0;
    virtual bool activateNativePreset(const QString& nativePresetId, qreal speed) = 0;

private:
    bool createNxPreset(const QnPtzPreset& preset,
        bool generateId = false,
        QString* outNxPresetId = nullptr);
    bool updateNxPreset(const QnPtzPreset& preset);
    bool removeNxPreset(const QString& preset);
    bool nxPresets(QMap<QString, QnPtzPresetRecord>* outNxPresets);
    bool activateNxPreset(const QString& preset, qreal speed);

    void createPresetMapping(const QString& nxId, const QString& nativeId);
    void removePresetMappings(const QString& nxId, const QString& nativeId);

    bool hasNxPreset(const QString& nxId) const;
    bool hasNativePreset(const QString& nativeId) const;
    QString nativePresetIdFromNxId(const QString& nxId) const;

    QString serializePresets(const QnPtzPresetRecordHash& presets);
    QnPtzPresetRecordHash deserializePresets(const QString& presetsSerialized);
    bool doPresetsAction(PresetsActionFunc actionFunc, const QnPtzPreset& preset = QnPtzPreset());
    bool removeObsoleteMappings(
        const QSet<QString>& obsoleteNativeIds,
        QSet<QString>* outRemovedNxPresets);

private:
    mutable QnMutex m_mutex;
    QnResourcePtr m_camera;
    QnAbstractPtzController* m_controller;
    QMap<QString, QString> m_deviceToNxMapping;
    QMap<QString, QString> m_nxToDeviceMapping;

    QSet<QString> m_nativePresets;
    QSet<QString> m_nxPresets;
};

} // namespace ptz
} // namespace mediaserver
} // namespace nx