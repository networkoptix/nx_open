#include "ptz_types.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzHotkey, (json), (id)(hotkey))

const int QnPtzHotkey::kNoHotkey = -1;

QnPtzHotkey::QnPtzHotkey():
    hotkey(kNoHotkey)
{
}

QnPtzHotkey::QnPtzHotkey(const QString &id, int hotkey):
    id(id),
    hotkey(hotkey)
{
}

bool deserialize(const QString& /*value*/, QnPtzIdByHotkeyHash* /*target*/)
{
    NX_ASSERT(false, Q_FUNC_INFO, "Not implemented");
    return false;
}


