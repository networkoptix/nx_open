#pragma once

#include <QtCore/QtGlobal>

#if defined(Q_OS_MACX) || defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(__aarch64__)
#include <zlib.h>
#else
#include <QtZlib/zlib.h>
#endif

#include "../byte_stream/abstract_byte_stream_filter.h"

namespace nx {
namespace utils {
namespace bstream {
namespace gzip {

/**
 * Deflates gzip-compressed stream. Suitable for decoding gzip http content encoding.
 */
class NX_UTILS_API Uncompressor:
    public AbstractByteStreamFilter
{
public:
    Uncompressor(
        const std::shared_ptr<AbstractByteStreamFilter>& nextFilter = nullptr);
    virtual ~Uncompressor();

    virtual bool processData(const QnByteArrayConstRef& data) override;
    virtual size_t flush() override;

private:
    enum class State
    {
        init,
        inProgress,
        done,
        failed
    };

    State m_state;
    z_stream m_zStream;
    QByteArray m_outputBuffer;
};

} // namespace gzip
} // namespace bstream
} // namespace utils
} // namespace nx
