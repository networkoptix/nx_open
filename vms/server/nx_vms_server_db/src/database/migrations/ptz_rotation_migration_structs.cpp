#include "ptz_rotation_migration_structs.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {
namespace migration {
namespace ptz {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(OldPtzPresetData, (json)(eq), OldPtzPresetData_Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(OldPtzPresetRecord, (json)(eq), OldPtzPresetRecord_Fields);

} // namespace ptz
} // namespace migration
} // namespace ec2
