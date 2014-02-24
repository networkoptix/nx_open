#include <utils/common/model_functions.h>

#include "client_model_types.h"

QN_DEFINE_STRUCT_FUNCTIONS(QnWorkbenchState, (datastream), (currentLayoutIndex)(layoutUuids));
QN_DEFINE_STRUCT_FUNCTIONS(QnServerStorageKey, (datastream)(eq)(hash), (serverUuid)(storagePath));
QN_DEFINE_STRUCT_FUNCTIONS(QnLicenseWarningState, (datastream), (lastWarningTime));
QN_DEFINE_STRUCT_FUNCTIONS(QnPtzHotkey, (json), (id)(hotkey))

