// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <vector>

#include <QtCore/QString>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "id_data.h"

namespace nx::vms::api {

/**
 * List of analytics object lookup entries, intended for the VMS Rules usage in the first place.
 * It is used to check whether found analytics object matches one of the entries (e.g. to maintain
 * banlist or allow-list in the Rule).
 * Entry contains list of attributes, which must match with the object. Set of attributes can
 * differ between entries, e.g. one entry can allow all Cars with subtype "Bus" while other will
 * contain allowed license plate numbers.
 */
struct LookupListData: IdData
{
    /**%apidoc
     * User-defined name of the list.
     * %example Parking Allow-list.
     */
    QString name;

    /**%apidoc
     * ID of type of objects which are stored in the list.
     * %example nx.base.Vehicle
     */
    QString objectTypeId;

    /**%apidoc
     * All potential attribute names in user-defined order (column headers actually). Can contain
     * nested attributes in the format: `attributeName.subAttributeName` if attribute type is
     * Object. Nesting chain has no depth limitation, names are joined using the dot.
     * Only attribute names from this list are allowed to be stored in the entries list.
     * %example ["Color", "License Plate.Number"]
     */
    std::vector<QString> attributeNames;

    /**%apidoc
     * List of entries. Each entry is a map from an attribute name to a corresponding value. If an
     * attribute has Object type, it's own attributes can also be stored here in the format:
     * `attributeName.subAttributeName`. Nesting chain has no depth limitation, names are joined
     * using the dot.
     * Only attributes from attributeNames field will be used (and should actually be here). All
     * attributes are optional.
     * %example [
     *    {"Color": "red"},
     *    {"License Plate.Number": "AA000A"},
     *    {"Color": "blue", "License Plate.Number": "BB777B"}
     * ]
     */
    std::vector<std::map<QString, QString>> entries;
};
#define LookupListData_Fields IdData_Fields(name)(objectTypeId)(attributeNames) (entries)
NX_REFLECTION_INSTRUMENT(LookupListData, LookupListData_Fields)

using LookupListDataList = std::vector<LookupListData>;
NX_VMS_API_DECLARE_STRUCT_EX(LookupListData, (ubjson)(json))

} // namespace nx::vms::api
