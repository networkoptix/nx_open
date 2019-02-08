#include "setup_local_system_data.h"

#include <nx/fusion/model_functions.h>

namespace {

static const QString kSystemNameParamName(QLatin1String("systemName"));

} // namespace

SetupLocalSystemData::SetupLocalSystemData()
{
}

SetupLocalSystemData::SetupLocalSystemData(const QnRequestParams& params):
    PasswordData(params),
    systemName(params.value(kSystemNameParamName))
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SetupLocalSystemData),
    (json),
    _Fields)
