#pragma once

#include <functional>
#include <stack>

namespace nx {
namespace utils {

/**
 * Queue that can return "top" element with O(1) time compexity.
 * Top element here is, for example, the largest element (it is defined by Comp functor).
 */
template<typename T, typename Comp = std::less<T>>
class TopQueue
{
public:
    using size_type = std::size_t;

    /** Running time - amortized O(1). */
    void push(const T& val);
    /**
     * @return Oldest element in the queue.
     */
    T& front();
    /** Running time - O(1). */
    void pop();
    /**
     * @return If std::less<> is used as Comp (default), then min element is returned.
     * NOTE: Running time - O(1).
     */
    const T& top() const;

    bool empty() const;
    size_type size() const;

private:
    // TODO: #ak Store "current top" as an reference to an existing element.
    // This will decrease requirements to type T to movable instead of copyable.
    std::stack<std::pair<T /*value*/, T /*current top*/>> m_pushStack;
    std::stack<std::pair<T /*value*/, T /*current top*/>> m_popStack;
    Comp m_comp;

    void populatePopStack();

    static const T& selectTopValue(const T& left, const T& right);
};

template<typename T, typename Comp>
void TopQueue<T, Comp>::push(const T& val)
{
    if (m_pushStack.empty())
    {
        m_pushStack.push(std::make_pair(val, val));
        return;
    }

    m_pushStack.push(std::make_pair(val, selectTopValue(val, m_pushStack.top().second)));
}

template<typename T, typename Comp>
T& TopQueue<T, Comp>::front()
{
    if (m_popStack.empty())
        populatePopStack();

    return m_popStack.top().first;
}

template<typename T, typename Comp>
void TopQueue<T, Comp>::pop()
{
    if (m_popStack.empty())
        populatePopStack();

    m_popStack.pop();
}

template<typename T, typename Comp>
const T& TopQueue<T, Comp>::top() const
{
    if (m_pushStack.empty())
        return m_popStack.top().second;
    else if (m_popStack.empty())
        return m_pushStack.top().second;
    else
        return selectTopValue(m_popStack.top().second, m_pushStack.top().second);
}

template<typename T, typename Comp>
bool TopQueue<T, Comp>::empty() const
{
    return m_pushStack.empty() && m_popStack.empty();
}

template<typename T, typename Comp>
typename TopQueue<T, Comp>::size_type TopQueue<T, Comp>::size() const
{
    return m_pushStack.size() + m_popStack.size();
}

template<typename T, typename Comp>
void TopQueue<T, Comp>::populatePopStack()
{
    while (!m_pushStack.empty())
    {
        const auto& elemToPush = m_pushStack.top().first;
        if (m_popStack.empty())
        {
            m_popStack.push(std::make_pair(elemToPush, elemToPush));
        }
        else
        {
            m_popStack.push(std::make_pair(
                elemToPush,
                selectTopValue(m_popStack.top().second, elemToPush)));
        }

        m_pushStack.pop();
    }
}

template<typename T, typename Comp>
const T& TopQueue<T, Comp>::selectTopValue(
    const T& left, const T& right)
{
    return Comp()(left, right) ? left : right;
}

} // namespace utils
} // namespace nx
