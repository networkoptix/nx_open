// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "layout_tour_data.h"

#include <QtCore/QString>

namespace nx::vms::api {

/**%apidoc
 * %param name
 *     %example Showreel 1
 */
struct NX_VMS_API ShowreelModel: LayoutTourData
{
    using DbReadTypes = std::tuple<LayoutTourData>;
    using DbUpdateTypes = std::tuple<LayoutTourData>;
    using DbListTypes = std::tuple<LayoutTourDataList>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<ShowreelModel> fromDbTypes(DbListTypes data);
};
#define ShowreelModel_Fields LayoutTourData_Fields
QN_FUSION_DECLARE_FUNCTIONS(ShowreelModel, (json), NX_VMS_API)

} // namespace nx::vms::api
