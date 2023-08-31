// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webpage_model.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(WebPageModel, (json), WebPageModel_Fields)

WebPageModel::DbUpdateTypes WebPageModel::toDbTypes() &&
{
    WebPageData data;
    data.id = std::move(id);
    data.name = std::move(name);
    data.url = std::move(url);

    auto parameters = asList(data.id);
    return {
        std::move(data),
        std::move(parameters),
    };
}

std::vector<WebPageModel> WebPageModel::fromDbTypes(DbListTypes all)
{
    return ResourceWithParameters::fillListFromDbTypes<WebPageModel, WebPageData>(
        &all,
        [](WebPageData data)
        {
            WebPageModel model;
            model.id = std::move(data.id);
            model.name = std::move(data.name);
            model.url = std::move(data.url);
            return model;
        });
}

} // namespace nx::vms::api
