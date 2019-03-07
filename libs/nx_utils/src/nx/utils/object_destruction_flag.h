#pragma once

#include <vector>
#include <thread>

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
public:
    enum ControlledObjectState
    {
        alive = 0,
        markedForDeletion,
        deleted,
        customState,
    };

    ObjectDestructionFlag();
    ~ObjectDestructionFlag();

    ObjectDestructionFlag(const ObjectDestructionFlag&) = delete;
    ObjectDestructionFlag& operator=(const ObjectDestructionFlag&) = delete;

    void markAsDeleted();
    void recordCustomState(ControlledObjectState state);

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

        /**
         * Effectively, does the same as Watcher::objectDestroyed.
         * TODO: But, the objectDestroyed behavior should be changed since this class handles
         * more than object destruction. But every usage would have to be updated.
         * So, object destruction is a specific case of an interruption.
         */
        bool interrupted() const;
        bool objectDestroyed() const;

    private:
        ControlledObjectState m_localValue;
        ObjectDestructionFlag* const m_objectDestructionFlag;
    };

private:
    std::vector<ControlledObjectState*> m_watcherStates;
    std::thread::id m_lastWatchingThreadId;
};

} // namespace nx
} // namespace utils
