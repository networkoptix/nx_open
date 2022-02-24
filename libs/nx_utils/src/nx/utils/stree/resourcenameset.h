// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <string>

#include <QtCore/QVariant>

namespace nx::utils::stree {

static const int INVALID_RES_ID = -1;

/**
 * NOTE: Resource id -1 is reserved for error reporting.
 * NOTE: Methods of this class are not thread-safe.
 */
class NX_UTILS_API ResourceNameSet
{
public:
    class ResourceDescription
    {
    public:
        int id = -1;
        std::string name;
        QVariant::Type type = QVariant::Invalid;

        ResourceDescription() = default;

        ResourceDescription(
            int _id,
            const std::string& _name,
            QVariant::Type _type)
            :
            id(_id),
            name(_name),
            type(_type)
        {
        }
    };

    /**
     * resID and resName MUST be unique.
     * @return true if resource has been registered.
     */
    bool registerResource(int resID, const std::string& resName, QVariant::Type type);

    /**
     * @return std::pair<resource id, resource type>. If no resource found returns INVALID_RES_ID.
     */
    ResourceDescription findResourceByName(const std::string& resName) const;

    /** 
     * Searches resource name by id resID. Empty string is returned in case of error.
     */
    ResourceDescription findResourceByID(int resID) const;
    void removeResource(int resID);

private:
    /**
     * map<resource name, ResourceDescription>.
     */
    using ResNameToIDDict = std::map<std::string, ResourceDescription>;

    /**
     * map<resource id, ResNameToIDDict::iterator>.
     */
    using ResIDToNameDict = std::map<int, ResNameToIDDict::iterator>;

    ResNameToIDDict m_resNameToID;
    ResIDToNameDict m_resIDToName;
};

} // namespace nx::utils::stree
