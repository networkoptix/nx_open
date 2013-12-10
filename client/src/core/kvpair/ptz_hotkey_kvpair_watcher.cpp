#include "ptz_hotkey_kvpair_watcher.h"

#include <QtCore/QList>

#include <core/ptz/ptz_hotkey.h>

#include <utils/common/json.h>

namespace {
    static const QLatin1String watcherId("ptz_hotkeys");
}

QnPtzHotkeyKvPairWatcher::QnPtzHotkeyKvPairWatcher(QObject *parent) :
    base_type(parent)
{
}

QnPtzHotkeyKvPairWatcher::~QnPtzHotkeyKvPairWatcher() {
}

QString QnPtzHotkeyKvPairWatcher::key() const {
    return watcherId;
}

void QnPtzHotkeyKvPairWatcher::updateValue(int resourceId, const QString &value) {
    //TODO: #GDM signals
    if(value.isEmpty()) {
        m_hotkeysByResourceId.remove(resourceId);
    } else {
        QnHotkeysHash hotkeys;
        if (!QJson::deserialize<QnHotkeysHash >(value.toUtf8(), &hotkeys))
            return;
        m_hotkeysByResourceId[resourceId] = hotkeys;
    }
}

void QnPtzHotkeyKvPairWatcher::removeValue(int resourceId) {
    //TODO: #GDM signals
    m_hotkeysByResourceId.remove(resourceId);
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
    submitValue(resourceId, QString::fromUtf8(QJson::serialized(hotkeys)));
}

