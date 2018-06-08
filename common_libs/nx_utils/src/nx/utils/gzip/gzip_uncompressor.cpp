#include "gzip_uncompressor.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/zlib.h>

namespace nx::utils::bstream::gzip {

static constexpr int kOutputBufferSize = 16 * 1024;

class Uncompressor::Private
{
public:
    enum class State
    {
        init,
        inProgress,
        done,
        failed
    };

    State state = State::init;
    z_stream zStream;
    QByteArray outputBuffer;
};

Uncompressor::Uncompressor(const std::shared_ptr<AbstractByteStreamFilter>& nextFilter):
    AbstractByteStreamFilter(nextFilter),
    d(new Private())
{
    memset(&d->zStream, 0, sizeof(d->zStream));
    d->outputBuffer.resize(kOutputBufferSize);
}

Uncompressor::~Uncompressor()
{
    inflateEnd(&d->zStream);
}

bool Uncompressor::processData(const QnByteArrayConstRef& data)
{
    if (data.isEmpty())
        return true;

    int zFlushMode = Z_NO_FLUSH;

    d->zStream.next_in = (Bytef*) data.constData();
    d->zStream.avail_in = (uInt) data.size();
    d->zStream.next_out = (Bytef*) d->outputBuffer.data();
    d->zStream.avail_out = (uInt) d->outputBuffer.size();

    for (;;)
    {
        int zResult = Z_OK;
        switch (d->state)
        {
            case Private::State::init:
            case Private::State::done: //< To support stream of gzipped files.
                zResult = inflateInit2(&d->zStream, 16 + MAX_WBITS);
                if (zResult != Z_OK)
                {
                    NX_ASSERT(false);
                }
                d->state = Private::State::inProgress;
                continue;

            case Private::State::inProgress:
            {
                // Note, assuming that inflate always eat all input stream if output buffer is
                // available.
                const uInt availInBak = d->zStream.avail_in;
                zResult = inflate(&d->zStream, zFlushMode);
                const uInt inBytesConsumed = availInBak - d->zStream.avail_in;

                switch (zResult)
                {
                    case Z_OK:
                    case Z_STREAM_END:
                    {
                        if (zResult == Z_STREAM_END)
                            d->state = Private::State::done;

                        if (d->zStream.avail_out == 0 && d->zStream.avail_in > 0)
                        {
                            // Setting new output buffer and calling inflate once again.
                            m_nextFilter->processData(d->outputBuffer);
                            d->zStream.next_out = (Bytef*) d->outputBuffer.data();
                            d->zStream.avail_out = (uInt) d->outputBuffer.size();
                            continue;
                        }
                        else if (d->zStream.avail_in == 0)
                        {
                            // Input depleted.
                            return m_nextFilter->processData(QnByteArrayConstRef(
                                d->outputBuffer,
                                0,
                                (uInt) d->outputBuffer.size() - d->zStream.avail_out));
                        }
                        else //< d->zStream.avail_out > 0 && d->zStream.avail_in > 0
                        {
                            if (d->zStream.avail_out < (uInt) d->outputBuffer.size())
                            {
                                m_nextFilter->processData(QnByteArrayConstRef(
                                    d->outputBuffer,
                                    0,
                                    (uInt) d->outputBuffer.size() - d->zStream.avail_out));
                                d->zStream.next_out = (Bytef*) d->outputBuffer.data();
                                d->zStream.avail_out = (uInt) d->outputBuffer.size();
                            }
                            else if (inBytesConsumed == 0)
                                //< && d->zStream.avail_out == d->outputBuffer.size()
                            {
                                // Zlib does not consume any input data and does not write anything
                                // to output.
                                d->state = Private::State::failed; //< To avoid inifinite loop.
                            }
                            continue;
                        }

                        break;
                    }

                    case Z_BUF_ERROR:
                        // This error is recoverable.
                        if (d->zStream.avail_in > 0)
                        {
                            // May be some more out buf is required?
                            if (d->zStream.avail_out < (uInt) d->outputBuffer.size())
                            {
                                m_nextFilter->processData(QnByteArrayConstRef(
                                    d->outputBuffer,
                                    0,
                                    (uInt) d->outputBuffer.size() - d->zStream.avail_out));
                                d->zStream.next_out = (Bytef*) d->outputBuffer.data();
                                d->zStream.avail_out = (uInt) d->outputBuffer.size();
                                continue; //< Trying with more output buffer.
                            }
                            else if (zFlushMode == Z_NO_FLUSH)
                            {
                                // Trying inflate with Z_SYNC_FLUSH.
                                zFlushMode = Z_SYNC_FLUSH;
                                continue;
                            }
                            else
                            {
                                // TODO: We should cache left input data and give it to inflate
                                // along with next input data portion.
                            }
                        }
                        else //< d->zStream.avail_in == 0
                        {
                            if (d->zStream.avail_out > 0)
                            {
                                m_nextFilter->processData(QnByteArrayConstRef(
                                    d->outputBuffer,
                                    0,
                                    (uInt) d->outputBuffer.size() - d->zStream.avail_out));
                            }
                            return true;
                        }
                        d->state = Private::State::failed;
                        break;

                    default:
                        d->state = Private::State::failed;
                        break;
                }
                break;
            }

            case Private::State::failed:
                break;

            default:
                NX_ASSERT(false);
        }

        break;
    }

    return true;
}

size_t Uncompressor::flush()
{
    d->zStream.next_in = nullptr;
    d->zStream.avail_in = 0;
    d->zStream.next_out = (Bytef*) d->outputBuffer.data();
    d->zStream.avail_out = (uInt) d->outputBuffer.size();

    int zResult = inflate(&d->zStream, Z_SYNC_FLUSH);
    if ((zResult == Z_OK || zResult == Z_STREAM_END)
        && (uInt) d->outputBuffer.size() > d->zStream.avail_out)
    {
        m_nextFilter->processData(QnByteArrayConstRef(
            d->outputBuffer, 0, (uInt) d->outputBuffer.size() - d->zStream.avail_out));
        return (uInt) d->outputBuffer.size() - d->zStream.avail_out;
    }
    return 0;
}

} // namespace nx::utils::bstream::gzip
