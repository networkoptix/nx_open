#pragma once 

#include "abstract_byte_stream_filter.h"

namespace nx {
namespace utils {
namespace bstream {

/**
 * Passes input data to specified function.
 */
template<class Func>
class CustomOutputStream:
    public AbstractByteStreamFilter
{
public:
    CustomOutputStream(const Func& func):
        m_func(func)
    {
    }

    virtual bool processData(const QnByteArrayConstRef& data)
    {
        // TODO: #ak Add support of functor with return value.
        m_func(data);
        return true;
    }

    virtual size_t flush() { return 0; }

private:
    Func m_func;
};

template<class Func>
std::shared_ptr<CustomOutputStream<Func>> makeCustomOutputStream(Func func)
{
    return std::make_shared<CustomOutputStream<Func>>(std::move(func));
}

/**
 * Calls specified func and passes all input data to the next filter.
 */
template<class Func>
class FilterWithFunc:
    public AbstractByteStreamFilter
{
public:
    FilterWithFunc(const Func& func):
        m_func(func)
    {
    }

    virtual bool processData(const QnByteArrayConstRef& data)
    {
        m_func();
        return nextFilter()->processData(data);
    }

    virtual size_t flush()
    {
        return nextFilter()->flush();
    }

private:
    Func m_func;
};

template<class Func>
std::shared_ptr<FilterWithFunc<Func>> makeFilterWithFunc(Func func)
{
    return std::make_shared<FilterWithFunc<Func>>(std::move(func));
}

//-------------------------------------------------------------------------------------------------

/**
 * Convenience function to simplify some usages.
 * E.g., to uncompress some buffer:
 * <pre><code>
 *   auto result = runBytesThroughFilter<gzip::Uncompressor>(compressedData);
 * </code></pre>
 */
template<typename Filter>
QByteArray runBytesThroughFilter(const QByteArray& data)
{
    QByteArray result;

    auto saveResultFilter = makeCustomOutputStream(
        [&result](const auto& data) { result += data; });

    Filter filter(saveResultFilter);
    filter.processData(data);
    filter.flush();

    return result;
}

} // namespace bstream
} // namespace utils
} // namespace nx
