// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

class SystemContext;

/**
 * Unary predicate which defines subset of cameras and devices will be displayed in the dialog.
 */
using ResourceFilter = nx::vms::common::ResourceFilter;

/** Unary predicate which defines whether resource is valid choice for selection or not. */
using ResourceValidator = ResourceFilter;

/**
 * Provides alert text in case if some invalid resources are checked. List of invalid
 * resources will be passed as parameter.
 */
using AlertTextProvider = std::function<QString(
    SystemContext* system, const QSet<QnResourcePtr>& resources, bool pinnedItemSelected)>;

} // namespace nx::vms::client::desktop
