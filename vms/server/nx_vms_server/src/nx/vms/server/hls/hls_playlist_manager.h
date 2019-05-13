#pragma once

#include <memory>
#include <vector>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <QtCore/QString>

namespace nx {
namespace vms::server {
namespace hls {

class AbstractPlaylistManager
{
public:
    class ChunkData
    {
    public:
        boost::optional<QString> alias;
        //!internal timestamp (micros). Not a calendar time
        quint64 startTimestamp;
        //!in micros
        quint64 duration;
        //!sequential number of this chunk
        unsigned int mediaSequence;
        //!if true, there is discontinuity between this chunk and preceding one
        bool discontinuity;

        ChunkData();
        ChunkData(
            quint64 _startTimestamp,
            quint64 _duration,
            unsigned int _mediaSequence,
            bool _discontinuity = false);
        ChunkData(
            const QString& _alias,
            unsigned int _mediaSequence,
            bool _discontinuity = false);
    };

    virtual ~AbstractPlaylistManager() = default;

    /**
     * Generates chunks and appends them to chunkList.
     * @param endOfStreamReached Can be NULL.
     * @return Number of chunks generated.
     */
    virtual size_t generateChunkList(
        std::vector<ChunkData>* const chunkList,
        bool* const endOfStreamReached) const = 0;
    //!Returns maximum stream bitrate in bps
    virtual int getMaxBitrate() const = 0;
};

//!Using std::shared_ptr for \a std::shared_ptr::unique()
typedef std::shared_ptr<AbstractPlaylistManager> AbstractPlaylistManagerPtr;

} // namespace hls
} // namespace vms::server
} // namespace nx
