// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "webpage_data.h"

namespace nx::vms::api {

/**%apidoc Web page information object. */
struct NX_VMS_API WebPageModelV1: ResourceWithParameters
{
    /**%apidoc[immutable] Web page unique id. */
    nx::Uuid id;

    /**%apidoc Web page name. */
    QString name;

    /**%apidoc Web page URL. */
    QString url;

    /**%apidoc[opt]
     * Id of the Server through which the page should be proxied, null id means no proxy.
     */
    nx::Uuid parentId;

    using DbReadTypes = std::tuple<WebPageData, ResourceParamWithRefDataList>;
    using DbUpdateTypes = std::tuple<WebPageData, ResourceParamWithRefDataList>;
    using DbListTypes = std::tuple<WebPageDataList, ResourceParamWithRefDataList>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<WebPageModelV1> fromDbTypes(DbListTypes data);
};
#define WebPageModelV1_Fields \
    ResourceWithParameters_Fields \
    (id)(name)(url)(parentId)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(WebPageModelV1, (json))
NX_REFLECTION_INSTRUMENT(WebPageModelV1, WebPageModelV1_Fields)

struct NX_VMS_API WebPageModelV3: WebPageModelV1
{
    /**%apidoc[opt] */
    bool certificateCheck = false;
    std::vector<QString> proxyDomainAllowList{};

    DbUpdateTypes toDbTypes() &&;
    static std::vector<WebPageModelV3> fromDbTypes(DbListTypes data);
};
#define WebPageModelV3_Fields WebPageModelV1_Fields (certificateCheck)(proxyDomainAllowList)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(WebPageModelV3, (json))
NX_REFLECTION_INSTRUMENT(WebPageModelV3, WebPageModelV3_Fields)

using WebPageModel = WebPageModelV3;

} // namespace nx::vms::api
