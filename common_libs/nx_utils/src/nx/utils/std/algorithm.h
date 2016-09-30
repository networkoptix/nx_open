#include <algorithm>

namespace nx {
namespace utils {

template<typename ForwardIt, typename T>
ForwardIt binary_find(ForwardIt first, ForwardIt last, const T& value)
{
    auto lbit = std::lower_bound(first, last, value);
    if (lbit != last && !(value < *lbit))
        return lbit;

    return last;
}

template<typename ForwardIt, typename T, typename Compare>
ForwardIt binary_find(ForwardIt first, ForwardIt last, const T& value, Compare comp)
{
    auto lbit = std::lower_bound(first, last, value, comp);
    if (lbit != last && !comp(value, *lbit))
        return lbit;

    return last;
}

}
}
