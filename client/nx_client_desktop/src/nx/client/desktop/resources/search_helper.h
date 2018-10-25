#pragma once

#include <map>

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>

namespace nx::client::desktop::resources::search_helper {

/**
 * Search parameter. Used to display search string in the corresponding form.
 */
enum class Parameter
{
    id,
    logicalId,
    name,
    url,
    mac,
    host,
    model,
    vendor,
    firmware,
    physicalId,
    roleName,
};

using Matches = std::map<Parameter, QString>;

/**
 * Check if search string allows to make a search.
 */
bool isSearchStringValid(const QString& searchString);

/**
 * Check if resource matches search string.
 */
bool matches(const QString& searchString, const QnResourcePtr& resource);

/**
 * Map of resource parameters, matching given search string, to their actual value.
 */
Matches matchValues(const QString& searchString,const QnResourcePtr& resource);

} // namespace nx::client::desktop::resources::search_helper
