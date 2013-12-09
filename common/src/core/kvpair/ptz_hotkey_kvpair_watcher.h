#ifndef PTZ_HOTKEY_KVPAIR_WATCHER_H
#define PTZ_HOTKEY_KVPAIR_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <core/kvpair/abstract_kvpair_watcher.h>

#include <utils/common/json.h>

typedef QHash<QString, int> QnHotkeysHash;

class QnPtzHotkeyKvPairWatcher : public QnAbstractKvPairWatcher
{
    Q_OBJECT

    typedef QnAbstractKvPairWatcher base_type;
public:
    struct PresetHotkey {
        QString id;
        int hotkey;
    };

    explicit QnPtzHotkeyKvPairWatcher(QObject *parent = 0);
    virtual ~QnPtzHotkeyKvPairWatcher();

    /**
     * @brief presetByHotkey
     * @param resourceId
     * @param hotkey
     * @return                      Preset id if any, an empty string otherwise.
     */
    QString presetIdByHotkey(int resourceId, int hotkey) const;

    /**
     * @brief hotkeyById
     * @param resourceId
     * @param presetId
     * @return                      Assigned hotkey as 0-9 (if any), -1 otherwise.
     */
    int hotkeyByPresetId(int resourceId, const QString &presetId) const;

    QnHotkeysHash allHotkeysByResourceId(int resourceId) const;

    void updateHotkeys(int resourceId, const QnHotkeysHash &hotkeys);

    virtual void updateValue(int resourceId, const QString &value) override;

    virtual void removeValue(int resourceId) override;
private:
    QHash<int, QnHotkeysHash> m_hotkeysByResourceId;
};

#endif // PTZ_HOTKEY_KVPAIR_WATCHER_H
