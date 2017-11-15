#pragma once

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
class ObjectDestructionFlag
{
    enum class ControlledObjectState
    {
        alive = 0,
        markedForDeletion,
        deleted,
    };

public:
    ObjectDestructionFlag():
        m_valuePtr(nullptr)
    {
    }

    ObjectDestructionFlag(const ObjectDestructionFlag&) = delete;
    ObjectDestructionFlag& operator=(const ObjectDestructionFlag&) = delete;

    ~ObjectDestructionFlag()
    {
        if (m_valuePtr)
            *m_valuePtr = ControlledObjectState::deleted;
    }

    void markAsDeleted()
    {
        if (m_valuePtr)
            *m_valuePtr = ControlledObjectState::markedForDeletion;
    }

    /**
     * NOTE: Objects operating with same flag must not overlap
     */
    class Watcher
    {
    public:
        Watcher(ObjectDestructionFlag* const flag):
            m_localValue(ControlledObjectState::alive),
            m_objectDestructionFlag(flag)
        {
            m_objectDestructionFlag->m_valuePtr = &m_localValue;
        }

        Watcher(const Watcher&) = delete;
        Watcher& operator=(const Watcher&) = delete;

        ~Watcher()
        {
            if (m_localValue == ControlledObjectState::deleted)
                return; //< Object has been destroyed and m_objectDestructionFlag is invalid.
            m_objectDestructionFlag->m_valuePtr = nullptr;
        }

        bool objectDestroyed() const
        {
            return m_localValue > ControlledObjectState::alive;
        }

    private:
        ControlledObjectState m_localValue;
        ObjectDestructionFlag* const m_objectDestructionFlag;
    };

private:
    ControlledObjectState* m_valuePtr;
};

} // namespace nx
} // namespace utils
