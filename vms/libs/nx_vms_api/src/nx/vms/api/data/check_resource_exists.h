// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::api {

/** Used by ...Model::toDbTypes() and transaction-description-modify checkers. */
enum class CheckResourceExists
{
    yes,
    no,
    customRole, /**< UserModel and ModifyAccessRightsChecker only. */
};

} // namespace nx::vms::api
