#include "ptz_hotkey_kvpair_adapter.h"

#include <core/resource/resource.h>
#include <utils/common/json.h>

namespace {
    const QString targetKey = lit("ptz_hotkeys");

    bool readHotkeys(const QString &encoded, QnPtzHotkeyHash* target) {
        return QJson::deserialize<QnPtzHotkeyHash >(encoded.toUtf8(), target);
    }

    bool readHotkeys(const QnResourcePtr &resource, QnPtzHotkeyHash* target) {
        QString encoded = resource->getProperty(targetKey);
        if (encoded.isEmpty())
            return false;
        return readHotkeys(encoded, target);
    }
}

QnPtzHotkeyKvPairAdapter::QnPtzHotkeyKvPairAdapter(const QnResourcePtr &resource, QObject *parent):
    base_type(parent)
{
    readHotkeys(resource, &m_hotkeys);

    connect(resource, &QnResource::propertyChanged, this, &QnPtzHotkeyKvPairAdapter::at_resource_propertyChanged);
}

int QnPtzHotkeyKvPairAdapter::hotkeyByPresetId(const QString &presetId) const {
    return m_hotkeys.key(presetId, QnPtzHotkey::NoHotkey);
}

int QnPtzHotkeyKvPairAdapter::hotkeyByPresetId(const QnResourcePtr &resource, const QString &presetId) {
    QnPtzHotkeyHash hotkeys;
    if (!readHotkeys(resource, &hotkeys))
        return QnPtzHotkey::NoHotkey;
    return hotkeys.key(presetId, QnPtzHotkey::NoHotkey);
}

QString QnPtzHotkeyKvPairAdapter::presetIdByHotkey(int hotkey) const {
    return m_hotkeys.value(hotkey);
}

QString QnPtzHotkeyKvPairAdapter::presetIdByHotkey(const QnResourcePtr &resource, int hotkey) {
    QnPtzHotkeyHash hotkeys;
    if (!readHotkeys(resource, &hotkeys))
        return QString();
    return hotkeys.value(hotkey);
}

QString QnPtzHotkeyKvPairAdapter::key() {
    return targetKey;
}

QnPtzHotkeyHash QnPtzHotkeyKvPairAdapter::hotkeys() const {
    return m_hotkeys;
}

void QnPtzHotkeyKvPairAdapter::at_resource_propertyChanged(const QnResourcePtr &resource, const QString &key) {
    if (key != targetKey)
        return;
    readHotkeys(resource, &m_hotkeys);
}

