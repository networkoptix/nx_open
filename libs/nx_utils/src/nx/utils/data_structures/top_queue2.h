#pragma once

#include <functional>
#include <deque>

namespace nx::utils {

/**
 * Queue that can return "top" element with O(1) time compexity.
 * Top element here is, for example, the largest element (it is defined by Comp functor).
 */
template <typename T, const T& (*Comp)(const T&, const T&)>
class TopQueue2
{
    struct Data
    {
        Data(const T& value, const T& topValue): value(value), topValue(topValue) {}
        T value;
        T topValue;
    };

public:
    TopQueue2(const T& bottomValue):
        m_bottomValue(bottomValue),
        m_currentTop(bottomValue),
        m_historySize(0)
    {
    }

    /** Running time - O(1). */
    void push_back(const T& value)
    {
        m_data.push_back(Data(value, m_bottomValue));
        m_currentTop = Comp(m_currentTop, value);
    }

    /** Running time - amortized O(1). */
    void pop_front()
    {
        m_data.pop_front();
        if (--m_historySize >= 0)
            return;
        update();
    }

    void update()
    {
        m_historySize = m_data.size();
        T tmpMax = m_currentTop = m_bottomValue;
        for (auto itr = m_data.rbegin(); itr != m_data.rend(); ++itr)
            itr->topValue = tmpMax = Comp(itr->value, tmpMax);
    }

    T top() const { return Comp(m_data.front().topValue, m_currentTop); }

    T& front() { return m_data.front().value; }
    T& last() { return m_data.rbegin()->value; }
    bool empty()  const { return m_data.empty(); }
private:
    T m_bottomValue;
    T m_currentTop;
    int m_historySize;
    std::deque<Data> m_data;
};

template <typename T>
class QueueWithMax: public TopQueue2<T, std::max<T>>
{
public:
    QueueWithMax(): TopQueue2<T, std::max<T>>(std::numeric_limits<T>::min()) {}
};

template <typename T>
class QueueWithMin: public TopQueue2<T, std::min<T>>
{
public:
    QueueWithMin(): TopQueue2 < T, std::min<T>>(std::numeric_limits<T>::max()) {}
};

} // namespace nx::utils
