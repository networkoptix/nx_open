// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_models.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkFilter, (json), BookmarkFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Bookmark, (json), Bookmark_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkWithRule, (json), BookmarkWithRule_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkTagFilter, (json), BookmarkTagFilter_Fields)

} // namespace nx::vms::api
