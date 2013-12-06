#include <utils/common/model_functions.h>

#include "client_model_types.h"

QN_DEFINE_STRUCT_FUNCTIONS(QnWorkbenchState, (datastream), (currentLayoutIndex)(layoutUuids), inline);
QN_DEFINE_STRUCT_FUNCTIONS(QnServerStorageKey, (datastream)(eq)(hash), (serverUuid)(storagePath), inline);
QN_DEFINE_STRUCT_FUNCTIONS(QnLicenseWarningState, (datastream), (lastWarningTime)(ignore), inline);




