#include "ptz_hotkey_kvpair_watcher.h"

#include <QtCore/QList>

#include <core/ptz/ptz_hotkey.h>

#include <utils/common/json.h>

QnPtzHotkeyKvPairWatcher::QnPtzHotkeyKvPairWatcher(QObject *parent) :
    base_type(parent)
{
}

QnPtzHotkeyKvPairWatcher::~QnPtzHotkeyKvPairWatcher() {
}

QString QnPtzHotkeyKvPairWatcher::key() {
    return QLatin1String("ptz_hotkeys");
}

QString QnPtzHotkeyKvPairWatcher::presetIdByHotkey(int resourceId, int hotkey) const {
    if (!m_hotkeysByResourceId.contains(resourceId))
        return QString();
    return m_hotkeysByResourceId[resourceId].key(hotkey, QString());
}

int QnPtzHotkeyKvPairWatcher::hotkeyByPresetId(int resourceId, const QString &presetId) const {
    if (!m_hotkeysByResourceId.contains(resourceId))
        return Qn::NoHotkey;
    return m_hotkeysByResourceId[resourceId].value(presetId, Qn::NoHotkey);
}

QnHotkeysHash QnPtzHotkeyKvPairWatcher::allHotkeysByResourceId(int resourceId) const {
    return m_hotkeysByResourceId.value(resourceId, QnHotkeysHash());
}

void QnPtzHotkeyKvPairWatcher::updateHotkeys(int resourceId, const QnHotkeysHash &hotkeys) {
    m_hotkeysByResourceId[resourceId] = hotkeys;
    //TODO: #GDM submit to EC
}

void QnPtzHotkeyKvPairWatcher::updateValue(int resourceId, const QString &value) {
    //TODO: #GDM signals
    if(value.isEmpty()) {
        m_hotkeysByResourceId.remove(resourceId);
    } else {
        QList<PtzPresetHotkey> hotkeys;
        if (!QJson::deserialize<QList<PtzPresetHotkey> >(value.toUtf8(), &hotkeys))
            return;

        QnHotkeysHash hash;
        foreach(PtzPresetHotkey hotkey, hotkeys)
            hash[hotkey.id] = hotkey.hotkey;
        m_hotkeysByResourceId[resourceId] = hash;
    }
}

void QnPtzHotkeyKvPairWatcher::removeValue(int resourceId) {
    //TODO: #GDM signals
    m_hotkeysByResourceId.remove(resourceId);
}
