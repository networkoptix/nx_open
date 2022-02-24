// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop::resources::search_helper {

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

using Matches = QMap<QString, QList<Parameter>>;

/**
 * Check if search string allows to make a search.
 */
bool isSearchStringValid(const QString& searchString);

/**
 * Check if one of the subgroups to which resource belongs contains search string entirely
 * (case insensitive).
 */
bool resourceGroupContainsString(const QString& searchString, const QnResourcePtr& resource);

/**
 * Check if resource matches search string.
 */
bool matches(const QString& searchString, const QnResourcePtr& resource);

/**
 * List of unique words from given string (case insensitive).
 */
QStringList uniqueSearchWords(const QString& searchString);

/**
 * Map that contains list of matching resource parameters for each search word (case insensitive).
 */
Matches matchSearchWords(const QStringList& searchWords, const QnResourcePtr& resource);

} // namespace nx::vms::client::desktop::resources::search_helper
