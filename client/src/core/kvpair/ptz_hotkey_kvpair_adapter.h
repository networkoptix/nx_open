#ifndef PTZ_HOTKEY_KVPAIR_ADAPTER_H
#define PTZ_HOTKEY_KVPAIR_ADAPTER_H

#include <QtCore/QObject>

#include <api/model/kvpair.h>
#include <core/resource/resource_fwd.h>
#include <core/ptz/ptz_hotkey.h>

class QnPtzHotkeyKvPairAdapter: public QObject {
    Q_OBJECT
public:
    explicit QnPtzHotkeyKvPairAdapter(const QnResourcePtr &resource, QObject *parent = 0);

    /**
     * @brief hotkeyById
     * @param presetId
     * @return                      Assigned hotkey as 0-9 (if any), -1 otherwise.
     */
    int hotkeyByPresetId(const QString &presetId) const;

    static int hotkeyByPresetId(const QnResourcePtr &resource, const QString &presetId);

    /**
     * @brief presetByHotkey
     * @param hotkey
     * @return                      Preset id if any, an empty string otherwise.
     */
    QString presetIdByHotkey(int hotkey) const;

    static QString presetIdByHotkey(const QnResourcePtr &resource, int hotkey);

    static QString key();
    QnHotkeysHash hotkeys() const;
private slots:
    void at_valueByKeyChanged(const QnResourcePtr &resource, const QnKvPair &kvPair);
    void at_valueByKeyRemoved(const QnResourcePtr &resource, const QString &key);

private:
    QnHotkeysHash m_hotkeys;
};

#endif // PTZ_HOTKEY_KVPAIR_ADAPTER_H
