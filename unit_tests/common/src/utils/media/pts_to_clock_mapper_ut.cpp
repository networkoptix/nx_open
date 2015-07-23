/**********************************************************
* Jul 23, 2015
* a.kolesnikov
***********************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <utils/media/pts_to_clock_mapper.h>


typedef PtsToClockMapper::pts_type pts_type;
typedef PtsToClockMapper::ts_type ts_type;

TEST( PtsToClockMapper, test )
{
    uint32_t bits = 1;
    uint32_t x = (1 << bits) - 1;

    uint32_t bits1 = 30;
    uint32_t bits30mask = (1 << bits1) - 1;

    uint32_t pts1 = bits30mask - 100;
    //uint32_t pts2 = 100;
    uint32_t pts2 = bits30mask - 300;

    uint32_t diff = (pts2 - pts1) & bits30mask;

    uint32_t absDiff = std::min<uint32_t>( (pts2 - pts1) & bits30mask, (pts1 - pts2) & bits30mask );
}

TEST( PtsToClockMapper, general )
{
    static const int PTS_FREQUENCY = 1000;
    static const size_t PTS_BITS = 12;
    static const size_t PTS_MASK = (1<<PTS_BITS) -1;

    PtsToClockMapper::TimeSynchronizationData timeSync;

    PtsToClockMapper ptsToClockMapper(
        PTS_FREQUENCY,
        PTS_BITS,
        &timeSync );

    static const pts_type MIN_PTS = 100;
    static const pts_type MAX_PTS = 100 * 1000;
    static const pts_type STEPS = 500;
    static const pts_type STEP_VALUE = (MAX_PTS - MIN_PTS) / STEPS;

    boost::optional<ts_type> firstTimestamp;
    for( pts_type pts = MIN_PTS; pts < MAX_PTS; pts += STEP_VALUE )
    {
        auto timestamp = ptsToClockMapper.getTimestamp( pts & PTS_MASK );
        if( !firstTimestamp )
            firstTimestamp = timestamp;
        const auto tsDiff = timestamp - firstTimestamp.get();
        const auto ptsDiff = pts - MIN_PTS;
        ASSERT_EQ( tsDiff, ptsDiff * PTS_FREQUENCY );
    }
}
