#pragma once

#include <vector>

namespace nx::utils
{

/**
* A simple basic Circular Buffer.
* Implemented only because boost::circular_buffer as of Boost 1.67
* triggers c++17 deprecated warnings which require project-wide suppression.
* Naming follows boost::circular_buffer style.
* Feel free to make it more feature-rich.
*/
template <class T>
class circular_buffer: private std::vector<T>
{
public:
    using size_type = typename std::vector<T>::size_type;

    circular_buffer(size_type buffer_capacity);

    /** Pushes element to the end of the buffer. */
    void push_back(const T& value);

    /** Write pop() if you need it. */

    /** Returns all elements in a line. */
    std::vector<T> get_vector() const;

    /** Returns current buffer size (may only grow). */
    size_type size() const;
    /** Returns maximal buffer size (never changes). */
    size_type max_size() const;

private:
    size_type m_max_size = 0; //< Cached to avoid extra calls.
    size_type m_begin = 0;
    size_type m_end = 0;
};

template<class T>
circular_buffer<T>::circular_buffer(size_type buffer_capacity):
    std::vector<T>(buffer_capacity),
    m_max_size(buffer_capacity)
{
}

template<class T>
void circular_buffer<T>::push_back(const T& value)
{
    (*this)[m_end] = value;
    m_end = (m_end + 1) % m_max_size;

    if (m_end == m_begin)
        m_begin = (m_begin + 1) % m_max_size;
}

template<class T>
std::vector<T> circular_buffer<T>::get_vector() const
{
    std::vector<T> result;
    result.reserve(size());

    if (m_begin > m_end)
    {
        result.insert(result.end(), data() + m_begin, data() + m_max_size);
        result.insert(result.end(), data(), data() + m_end);
    }
    else
    {
        result.insert(result.end(), data() + m_begin, data() + m_end);
    }
    return result;
}

template<class T>
typename circular_buffer<T>::size_type circular_buffer<T>::size() const
{
    return (m_end + m_max_size - m_begin) % m_max_size;
}

template<class T>
typename circular_buffer<T>::size_type circular_buffer<T>::max_size() const
{
    return m_max_size;
}

} // namespace nx::utils
