// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rect_as_string.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_DEFINE_FUNCTIONS(RectAsString, (json_lexical))

std::string RectAsString::toString() const
{
    return QnLexical::serialized(*this).toStdString();
}

bool fromString(const std::string& str, RectAsString* target)
{
    return QnLexical::deserialize(QString::fromStdString(str), target);
}

} // namespace nx::vms::api
