// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "webpage_data.h"

namespace nx::vms::api {

/**%apidoc Web page information object. */
struct NX_VMS_API WebPageModel: ResourceWithParameters
{
    WebPageModel() = default;

    /**%apidoc[immutable] Web page unique id. */
    QnUuid id;

    /**%apidoc Web page name. */
    QString name;

    /**%apidoc Web page URL. */
    QString url;

    using DbReadTypes = std::tuple<
        WebPageData,
        ResourceParamWithRefDataList>;

    using DbUpdateTypes = std::tuple<
        WebPageData,
        ResourceParamWithRefDataList>;

    using DbListTypes = std::tuple<
        WebPageDataList,
        ResourceParamWithRefDataList>;

    QnUuid getId() const { return id; }
    void setId(QnUuid value) { id = std::move(value); }
    static_assert(isCreateModelV<WebPageModel>);
    static_assert(isUpdateModelV<WebPageModel>);
    static_assert(isFlexibleIdModelV<WebPageModel>);

    DbUpdateTypes toDbTypes() &&;
    static std::vector<WebPageModel> fromDbTypes(DbListTypes data);
};
#define WebPageModel_Fields IdData_Fields (id)(name)(url)(parameters)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(WebPageModel, (json))

} // namespace nx::vms::api
