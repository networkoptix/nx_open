// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include "showreel_data.h"

namespace nx::vms::api {

/**%apidoc
 * %param name
 *     %example Showreel 1
 */
struct NX_VMS_API ShowreelModel: ShowreelData
{
    using DbReadTypes = std::tuple<ShowreelData>;
    using DbUpdateTypes = std::tuple<ShowreelData>;
    using DbListTypes = std::tuple<ShowreelDataList>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<ShowreelModel> fromDbTypes(DbListTypes data);
};
#define ShowreelModel_Fields ShowreelData_Fields
QN_FUSION_DECLARE_FUNCTIONS(ShowreelModel, (json), NX_VMS_API)

} // namespace nx::vms::api
