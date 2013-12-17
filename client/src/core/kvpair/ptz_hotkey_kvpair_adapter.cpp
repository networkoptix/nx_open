#include "ptz_hotkey_kvpair_adapter.h"

#include <core/resource/resource.h>
#include <utils/common/json.h>

namespace {
    static const QString targetKey = lit("ptz_hotkeys");

    bool readHotkeys(const QString &encoded, QnHotkeysHash* target) {
        return QJson::deserialize<QnHotkeysHash >(encoded.toUtf8(), target);
    }

    bool readHotkeys(const QnResourcePtr &resource, QnHotkeysHash* target) {
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
    return m_hotkeys.value(presetId, Qn::NoHotkey);
}

int QnPtzHotkeyKvPairAdapter::hotkeyByPresetId(const QnResourcePtr &resource, const QString &presetId) {
    QnHotkeysHash hotkeys;
    if (!readHotkeys(resource, &hotkeys))
        return Qn::NoHotkey;
    return hotkeys.value(presetId, Qn::NoHotkey);
}

QString QnPtzHotkeyKvPairAdapter::presetIdByHotkey(int hotkey) const {
    return m_hotkeys.key(hotkey);
}

QString QnPtzHotkeyKvPairAdapter::presetIdByHotkey(const QnResourcePtr &resource, int hotkey) {
    QnHotkeysHash hotkeys;
    if (!readHotkeys(resource, &hotkeys))
        return QString();
    return hotkeys.key(hotkey);
}

QString QnPtzHotkeyKvPairAdapter::key() {
    return targetKey;
}

QnHotkeysHash QnPtzHotkeyKvPairAdapter::hotkeys() const {
    return m_hotkeys;
}

void QnPtzHotkeyKvPairAdapter::at_resource_propertyChanged(const QnResourcePtr &resource, const QString &key) {
    if (key != targetKey)
        return;
    readHotkeys(resource, &m_hotkeys);
}

