#include "abstract_byte_stream_filter.h"

namespace nx {
namespace utils {
namespace bstream {

AbstractByteStreamFilter::AbstractByteStreamFilter(
    const std::shared_ptr<AbstractByteStreamFilter>& nextFilter)
    :
    m_nextFilter(nextFilter)
{
}

size_t AbstractByteStreamFilter::flush()
{
    if (!m_nextFilter)
        return 0;
    return m_nextFilter->flush();
}

void AbstractByteStreamFilter::setNextFilter(
    const std::shared_ptr<AbstractByteStreamFilter>& nextFilter)
{
    m_nextFilter = nextFilter;
}

const std::shared_ptr<AbstractByteStreamFilter>& AbstractByteStreamFilter::nextFilter() const
{
    return m_nextFilter;
}

//-------------------------------------------------------------------------------------------------
// Utilities.

AbstractByteStreamFilterPtr last(const AbstractByteStreamFilterPtr& beginPtr)
{
    //using AbstractByteStreamFilterPtr* to avoid reference counter modifications
    const AbstractByteStreamFilterPtr* curElem = &beginPtr;
    while ((*curElem)->nextFilter())
        curElem = &((*curElem)->nextFilter());
    return *curElem;
}

AbstractByteStreamFilterPtr insert(
    const AbstractByteStreamFilterPtr& beginPtr,
    const AbstractByteStreamFilterPtr& _where,
    AbstractByteStreamFilterPtr what)
{
    if (_where == beginPtr)
    {
        what->setNextFilter(beginPtr);
        return what;
    }

    const AbstractByteStreamFilterPtr* beforeWhere = &beginPtr;
    for (;; )
    {
        if ((*beforeWhere)->nextFilter() == _where)
            break;
        beforeWhere = &((*beforeWhere)->nextFilter());
    }
    what->setNextFilter(_where);
    (*beforeWhere)->setNextFilter(what);
    return beginPtr;
}

} // namespace bstream
} // namespace utils
} // namespace nx
