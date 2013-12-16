#include "ptz_hotkey_kvpair_adapter.h"

#include <core/resource/resource.h>
#include <utils/common/json.h>

namespace {
    static const QLatin1String target_key("ptz_hotkeys");

    bool readHotkeys(const QString &encoded, QnHotkeysHash* target) {
        return QJson::deserialize<QnHotkeysHash >(encoded.toUtf8(), target);
    }

    bool readHotkeys(const QnResourcePtr &resource, QnHotkeysHash* target) {
        QString encoded = resource->getValueByKey(target_key);
        if (encoded.isEmpty())
            return false;
        return readHotkeys(encoded, target);
    }
}

QnPtzHotkeyKvPairAdapter::QnPtzHotkeyKvPairAdapter(const QnResourcePtr &resource, QObject *parent):
    QObject(parent)
{
    readHotkeys(resource, &m_hotkeys);
    connect(resource.data(), SIGNAL(valueByKeyChanged(QnResourcePtr, QnKvPair)), this, SLOT(at_valueByKeyChanged(QnResourcePtr,QnKvPair)));
    connect(resource.data(), SIGNAL(valueByKeyRemoved(QnResourcePtr, QString)), this, SLOT(at_valueByKeyRemoved(QnResourcePtr,QString)));
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
    return target_key;
}

QnHotkeysHash QnPtzHotkeyKvPairAdapter::hotkeys() const {
    return m_hotkeys;
}

void QnPtzHotkeyKvPairAdapter::at_valueByKeyChanged(const QnResourcePtr &resource, const QnKvPair &kvPair) {
    Q_UNUSED(resource)
    if (kvPair.name() != target_key)
        return;
    readHotkeys(kvPair.value(), &m_hotkeys);
}

void QnPtzHotkeyKvPairAdapter::at_valueByKeyRemoved(const QnResourcePtr &resource, const QString &key) {
    Q_UNUSED(resource)
    if (key != target_key)
        return;
    m_hotkeys.clear();
}
