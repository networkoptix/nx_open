#pragma once

#include <type_traits>
#include <utility>
#include <memory>

#include <QtCore/QObject>

namespace nx::vms::client::desktop {

/*
 *  A set of static methods to attach/detach a companion object to/from another object.
 *   A companion is identified by string id. Pointer to companion is stored in a dynamic property.
 */
class ObjectCompanionManager
{
public:
    /** Returns an attached companion: */
    static QObject* companion(QObject* parent, const char* id);

    /** Attaches a companion to `parent` and returns previously attached companion.
    * Parent takes ownership of the new companion and releases ownership of the old one. */
    static std::unique_ptr<QObject> attach(QObject* parent, QObject* companion, const char* id);

    /** Detaches a companion and returns it (or null if no companion was attached).
    * Parent releases ownership of the companion. */
    static std::unique_ptr<QObject> detach(QObject* parent, const char* id);

    /** Detaches and deletes a companion (returns false if no companion was attached): */
    static bool uninstall(QObject* parent, const char* id);
};

/*
 * Generic object with static methods to create an instance and attach it as a companion to another object.
 */
template<class Base>
class ObjectCompanion:
    public Base,
    public ObjectCompanionManager
{
    static_assert(std::is_base_of<QObject, Base>::value,
        "ObjectCompanion must be specialized with a class derived from QObject");

    ObjectCompanion() = delete;

public:
    template<class ParentType>
    explicit ObjectCompanion(ParentType* parent): Base(parent) {}

    /** Creates and attaches a new companion and returns a pointer to it.
    * Parent takes ownership of the companion.
    * If an old companion exists, deletes it if `unique` is false,
    * or keeps it and returns nullptr if `unique` is true.
    */
    template<class ParentType>
    static ObjectCompanion<Base>* install(ParentType* parent, const char* id, bool unique)
    {
        if (unique && ObjectCompanionManager::companion(parent, id))
            return nullptr;

        auto result = new ObjectCompanion<Base>(parent);
        ObjectCompanionManager::attach(parent, result, id);
        return result;
    }
};

} // namespace nx::vms::client::desktop
