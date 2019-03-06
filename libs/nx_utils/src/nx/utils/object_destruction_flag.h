#pragma once

#include <vector>

namespace nx {
namespace utils {

/**
 * ObjectDestructionFlag and ObjectDestructionFlag::Watcher are used to allow
 * "this" destruction in some event handler.
 * Example:
 * @code{.cpp}
 * class SomeProtocolClient
 * {
 *     //..
 * private:
 *     //...
 *     ObjectDestructionFlag m_destructionFlag;
 * }

 * void SomeProtocolClient::readFromSocket(...)
 * {
 *     //...
 *     ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
 *     eventHandler(); //this handler can free this object
 *     if (watcher.objectDestroyed())
 *         return; //"this" has been destroyed, but watcher is still valid sine created on stack
 *     //...
 * }
 * @endcode
 */
class NX_UTILS_API ObjectDestructionFlag
{
    enum class ControlledObjectState
    {
        alive = 0,
        markedForDeletion,
        deleted,
    };

public:
    ObjectDestructionFlag();
    ~ObjectDestructionFlag();

    ObjectDestructionFlag(const ObjectDestructionFlag&) = delete;
    ObjectDestructionFlag& operator=(const ObjectDestructionFlag&) = delete;

    void markAsDeleted();

    /**
     * NOTE: Multiple objects using same ObjectDestructionFlag instance can be stacked.
     */
    class NX_UTILS_API Watcher
    {
    public:
        Watcher(ObjectDestructionFlag* const flag);
        ~Watcher();

        Watcher(const Watcher&) = delete;
        Watcher& operator=(const Watcher&) = delete;
        Watcher(Watcher&&) = default;
        Watcher& operator=(Watcher&&) = default;

        bool objectDestroyed() const;

    private:
        ControlledObjectState m_localValue;
        ObjectDestructionFlag* const m_objectDestructionFlag;
    };

private:
    std::vector<ControlledObjectState*> m_watcherStates;
};

} // namespace nx
} // namespace utils
