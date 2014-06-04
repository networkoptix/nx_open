#include <utils/common/model_functions.h>
#include <utils/common/enum_name_mapper.h>

#include "client_globals.h"
#include "client_model_types.h"

QN_DEFINE_EXPLICIT_ENUM_NAME_MAPPING(Qn::ClientSkin,
    ((Qn::LightSkin, "LightSkin"))
    ((Qn::DarkSkin, "DarkSkin"))
);

QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::ClientSkin)

QN_DEFINE_STRUCT_FUNCTIONS(QnWorkbenchState, (datastream), (currentLayoutIndex)(layoutUuids));
QN_DEFINE_STRUCT_FUNCTIONS(QnServerStorageKey, (datastream)(eq)(hash), (serverUuid)(storagePath));
QN_DEFINE_STRUCT_FUNCTIONS(QnLicenseWarningState, (datastream), (lastWarningTime));
QN_DEFINE_STRUCT_FUNCTIONS(QnPtzHotkey, (json), (id)(hotkey))


