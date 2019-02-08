#pragma once

#include <map>

#include <QtCore/QString>
#include <QtCore/QVariant>

namespace nx {
namespace utils {
namespace stree {

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
        int id;
        QString name;
        QVariant::Type type;

        ResourceDescription()
            :
            id(-1),
            type(QVariant::Invalid)
        {
        }

        ResourceDescription(
            int _id,
            QString _name,
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
    bool registerResource(int resID, const QString& resName, QVariant::Type type);
    /**
     * @return std::pair<resource id, resource type>. If no resource found returns INVALID_RES_ID.
     */
    ResourceDescription findResourceByName(const QString& resName) const;
    /** 
     * Searches resource name by id resID. Empty string is returned in case of error.
     */
    ResourceDescription findResourceByID(int resID) const;
    void removeResource(int resID);

private:
    /**
     * map<resource name, ResourceDescription>.
     */
    typedef std::map<QString, ResourceDescription > ResNameToIDDict;
    /**
     * map<resource id, ResNameToIDDict::iterator>.
     */
    typedef std::map<int, ResNameToIDDict::iterator> ResIDToNameDict;

    ResNameToIDDict m_resNameToID;
    ResIDToNameDict m_resIDToName;
};

} // namespace stree
} // namespace utils
} // namespace nx
