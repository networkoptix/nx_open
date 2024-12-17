// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include "../rules_fwd.h"

namespace nx::vms::api::rules { struct Rule; }

namespace nx::vms::rules::utils {

/** Encode rule info into short locale-independent format to store in the DB. */
NX_VMS_RULES_API QString toAuditLogFormat(const nx::vms::api::rules::Rule& rule);

/** Encode rule info into short locale-independent format to store in the DB. */
NX_VMS_RULES_API QString toAuditLogFormat(const ConstRulePtr& rule);

/** Decode rule info to human-readable form. */
NX_VMS_RULES_API QString detailedTextFromAuditLogFormat(const QString& encoded, Engine* engine);

} // namespace nx::vms::rules::utils
