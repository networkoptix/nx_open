#ifndef QN_PTZ_HOTKEY_H
#define QN_PTZ_HOTKEY_H

#include <QtCore/QString>
#include <QtCore/QHash>

#include <utils/common/json_fwd.h>

// can be placed in more common place if needed
namespace Qn {
    static const int NoHotkey = -1;
}

struct QnPtzPresetHotkey {
    QString id;
    int hotkey;

    QnPtzPresetHotkey(): hotkey(Qn::NoHotkey) {}
    QnPtzPresetHotkey(const QString &id, int hotkey): id(id), hotkey(hotkey) {}
};

typedef QHash<QString, int> QnHotkeysHash;

class QnAbstractPtzHotkeyDelegate {
public:
    virtual QnHotkeysHash hotkeys() const = 0;
    virtual void updateHotkeys(const QnHotkeysHash &value) = 0;
};

QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnPtzPresetHotkey)

#endif // QN_PTZ_HOTKEY_H
