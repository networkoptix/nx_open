// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "gzip_uncompressor.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/zlib.h>

namespace nx::utils::bstream::gzip {

static constexpr int kOutputBufferSize = 16 * 1024;
static constexpr int kDeflateWindowSize = 15; //< Max window size.

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
    Buffer outputBuffer;
    Method method = Method::gzip;
};

Uncompressor::Uncompressor(
    const std::shared_ptr<AbstractByteStreamFilter>& nextFilter,
    Method method)
    :
    AbstractByteStreamFilter(nextFilter),
    d(new Private())
{
    memset(&d->zStream, 0, sizeof(d->zStream));
    d->outputBuffer.resize(kOutputBufferSize);
    d->method = method;
}

Uncompressor::~Uncompressor()
{
    inflateEnd(&d->zStream);
}

bool Uncompressor::processData(const ConstBufferRefType& data)
{
    if (data.empty())
        return true;

    bool isFirstInflateAttempt = true;
    int zFlushMode = Z_NO_FLUSH;

    d->zStream.next_in = (Bytef*) data.data();
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
                // 32 is added for automatic zlib header detection, see
                // http://www.zlib.net/manual.html#Advanced.
                inflateEnd(&d->zStream);
                zResult = inflateInit2(
                    &d->zStream,
                    d->method == Method::gzip ? (32 + MAX_WBITS) : -kDeflateWindowSize);
                NX_ASSERT(zResult == Z_OK);
                d->state = Private::State::inProgress;
                continue;

            case Private::State::inProgress:
            {
                // Note, assuming that inflate always eat all input stream if output buffer is
                // available.
                const uInt availInBak = d->zStream.avail_in;
                zResult = inflate(&d->zStream, zFlushMode);

                if (isFirstInflateAttempt)
                {
                    isFirstInflateAttempt = false;
                    // Some servers seem to not generate zlib headers, try to decompress
                    // headless. This code is similar to the code in the curl utility.
                    if (zResult == Z_DATA_ERROR)
                    {
                        inflateEnd(&d->zStream);
                        d->zStream.next_in = (Bytef*) data.data();
                        d->zStream.avail_in = (uInt) data.size();
                        // Using negative windowBits parameter turns off looking for
                        // header, see http://www.zlib.net/manual.html#Advanced
                        zResult = inflateInit2(&d->zStream, -MAX_WBITS);
                        NX_ASSERT(zResult == Z_OK);
                        continue;
                    }
                }

                const uInt inBytesConsumed = availInBak - d->zStream.avail_in;

                switch (zResult)
                {
                    case Z_OK:
                    case Z_STREAM_END:
                    {
                        if (zResult == Z_STREAM_END)
                        {
                            d->state = Private::State::done;
                            inflateReset(&d->zStream);
                        }

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
                            return m_nextFilter->processData(ConstBufferRefType(
                                d->outputBuffer.data(),
                                (uInt) d->outputBuffer.size() - d->zStream.avail_out));
                        }
                        else //< d->zStream.avail_out > 0 && d->zStream.avail_in > 0
                        {
                            if (d->zStream.avail_out < (uInt) d->outputBuffer.size())
                            {
                                m_nextFilter->processData(ConstBufferRefType(
                                    d->outputBuffer.data(),
                                    (uInt) d->outputBuffer.size() - d->zStream.avail_out));
                                d->zStream.next_out = (Bytef*) d->outputBuffer.data();
                                d->zStream.avail_out = (uInt) d->outputBuffer.size();
                            }
                            else if (inBytesConsumed == 0)
                                //< && d->zStream.avail_out == d->outputBuffer.size()
                            {
                                // Zlib does not consume any input data and does not write anything
                                // to output.
                                d->state = Private::State::failed; //< To avoid infinite loop.
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
                                m_nextFilter->processData(ConstBufferRefType(
                                    d->outputBuffer.data(),
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
                                m_nextFilter->processData(ConstBufferRefType(
                                    d->outputBuffer.data(),
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
        m_nextFilter->processData(ConstBufferRefType(
            d->outputBuffer.data(),
            (uInt) d->outputBuffer.size() - d->zStream.avail_out));
        return (uInt) d->outputBuffer.size() - d->zStream.avail_out;
    }

    return 0;
}

} // namespace nx::utils::bstream::gzip
