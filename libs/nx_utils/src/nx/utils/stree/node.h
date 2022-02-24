// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "resourcecontainer.h"

#include <nx/utils/log/log.h>

/**
 * Contains implementation of simple search tree.
 */
namespace nx::utils::stree {

/**
 * Base class for all nodes.
 * NOTE: Descendant is not required to be thread-safe.
 */
class NX_UTILS_API AbstractNode
{
public:
    virtual ~AbstractNode() {}

    /**
     * Do node processing. This method can put some value to out and/or call get of some child node.
     */
    virtual void get(const AbstractResourceReader& in, AbstractResourceWriter* const out) const = 0;
    /**
     * Add child node. Takes ownership of child in case of success.
     * Implementation is allowed to reject adding child. In this case it must return false.
     * If noded added, true is returned and ownership of child object is taken.
     */
    virtual bool addChild(const QVariant& value, std::unique_ptr<AbstractNode> child) = 0;
};

/**
 * Iterates through all its children in order, defined by child value, cast to signed int.
 * NOTE: Children with same value are iterated in order they were added.
 */
class NX_UTILS_API SequenceNode:
    public AbstractNode
{
public:
    SequenceNode() = default;
    SequenceNode(SequenceNode&&) = delete;
    SequenceNode& operator=(SequenceNode&&) = delete;
    SequenceNode(const SequenceNode&) = delete;
    SequenceNode& operator=(const SequenceNode&) = delete;

    virtual void get(const AbstractResourceReader& in, AbstractResourceWriter* const out) const override;
    virtual bool addChild(const QVariant& value, std::unique_ptr<AbstractNode> child) override;

private:
    std::multimap<int, std::unique_ptr<AbstractNode>> m_children;
};

template<typename Key>
class DefaultKeyConverter
{
public:
    typedef Key KeyType;
    typedef Key SearchValueType;

    static Key convertToKey(const QVariant& value)
    {
        return value.value<Key>();
    }

    static Key convertToSearchValue(const QVariant& value)
    {
        return value.value<Key>();
    }
};

/**
 * Chooses child node to follow based on some condition.
 * All children have unique values. Attempt to add children with existing value will fail.
 * @param ConditionContainer must be associative dictionary from std::pair<Value, AbstractNode*>.
 * ConditionContainer::mapped_type MUST be AbstractNode*.
 * Example of ConditionContainer is std::map.
 */
template<
    typename Key,
    template<typename, typename> class ConditionContainer,
    typename KeyConversionFunc = DefaultKeyConverter<Key>>
class ConditionNode:
    public AbstractNode
{
    typedef ConditionContainer<Key, std::unique_ptr<AbstractNode>> Container;

public:
    ConditionNode(int matchResID):
        m_matchResId(matchResID)
    {
    }

    virtual void get(
        const AbstractResourceReader& in,
        AbstractResourceWriter* const out) const override
    {
        NX_TRACE(this, "Condition. Selecting child by resource %1", m_matchResId);

        QVariant value;
        if (!in.get(m_matchResId, &value))
        {
            NX_TRACE(this, "Condition. Resource (%1) not found in input data", m_matchResId);
            return;
        }

        const typename KeyConversionFunc::SearchValueType& typedValue =
            KeyConversionFunc::convertToSearchValue(value);
        typename Container::const_iterator it = m_children.find(typedValue);
        if (it == m_children.end())
        {
            NX_TRACE(this, "Condition. Could not find child by value %1", value);
            return;
        }

        NX_TRACE(this, nx::format("Condition. Found child with value %1 by search value %2")
            .arg(it->first).arg(value.toString()));
        it->second->get(in, out);
    }

    virtual bool addChild(
        const QVariant& value,
        std::unique_ptr<AbstractNode> child) override
    {
        return m_children.emplace(
            KeyConversionFunc::convertToKey(value),
            std::move(child)).second;
    }

private:
    Container m_children;
    int m_matchResId;
};

/**
 * Checks presense of specified resource in input container.
 * Allows only 2 children: false and true.
 */
class NX_UTILS_API ResPresenceNode:
    public AbstractNode
{
public:
    ResPresenceNode(int matchResID);

    virtual void get(const AbstractResourceReader& in, AbstractResourceWriter* const out) const override;
    virtual bool addChild(const QVariant& value, std::unique_ptr<AbstractNode> child) override;

private:
    /** [0] - for false. [1] - for true. */
    std::unique_ptr<AbstractNode> m_children[2];
    int m_matchResId;
};

/**
 * Puts some resource value to output.
 */
class NX_UTILS_API SetNode:
    public AbstractNode
{
public:
    SetNode(
        int resourceID,
        const QVariant& valueToSet);

    /**
     * Adds to out resource and then calls m_child->get() (if child exists).
     */
    virtual void get(const AbstractResourceReader& in, AbstractResourceWriter* const out) const override;
    virtual bool addChild(const QVariant& value, std::unique_ptr<AbstractNode> child) override;

private:
    std::unique_ptr<AbstractNode> m_child;
    int m_resourceID;
    const QVariant m_valueToSet;
};

} // namespace nx::utils::stree
