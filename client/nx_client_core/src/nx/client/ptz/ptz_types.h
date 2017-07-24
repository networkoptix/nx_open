#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>

struct QnPtzHotkey
{
    static const int kNoHotkey;

    QnPtzHotkey();
    QnPtzHotkey(const QString &id, int hotkey);

    QString id;
    int hotkey;
};

using QnPtzIdByHotkeyHash = QHash<int, QString>;
QN_FUSION_DECLARE_FUNCTIONS(QnPtzHotkey, (json)(metatype));

bool deserialize(const QString& value, QnPtzIdByHotkeyHash* target);
