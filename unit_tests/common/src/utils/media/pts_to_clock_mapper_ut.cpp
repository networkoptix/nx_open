/**********************************************************
* Jul 23, 2015
* a.kolesnikov
***********************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <utils/media/pts_to_clock_mapper.h>


typedef uint64_t pts_type;
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

    typedef PtsToClockMapper MapperType;

    for( int j = 0; j < 100; ++j )
    {
        srand( ::time(NULL) );

        MapperType::TimeSynchronizationData timeSync;

        MapperType ptsToClockMapper(
            PTS_FREQUENCY,
            PTS_BITS,
            &timeSync );

        static const pts_type MIN_PTS = 100;
        static const pts_type MAX_PTS = 100 * 1000;
        //static const pts_type STEPS = 500;
        //static const pts_type STEP_VALUE = (MAX_PTS - MIN_PTS) / STEPS;
        static const pts_type STEP_VALUE = 350;
        static const pts_type STEPS = (MAX_PTS - MIN_PTS) / STEP_VALUE;

        MapperType::ts_type firstTimestamp = -1;
        MapperType::ts_type prevTimestamp = -1;
        pts_type pts = MIN_PTS;
        pts_type prevEffectivePts = 0;
        pts_type ptsDelta = 0;

        ptsToClockMapper.updateTimeMapping( MIN_PTS, rand() );

        //for( pts_type pts = MIN_PTS; pts < MAX_PTS; pts += STEP_VALUE )
        for( int i = 0; i < STEPS; ++i )
        {

            auto effectivePts = pts;

            bool nonMonotonic = false;
            bool discontinuity = false;
            if( i > 0 )
            {
                if( rand() % 7 == 0 )
                {
                    //emulating non-monotonic pts 
                    effectivePts -= STEP_VALUE + STEP_VALUE/2;
                    nonMonotonic = true;
                }
                else if( rand() % 15 == 0 )
                {
                    //emulation pts discontinuity
                    ptsDelta -= pts - prevEffectivePts; //rolling back to prevEffectivePts

                    pts += PTS_MASK / 2;
                    effectivePts = pts;
                    ptsDelta += ptsToClockMapper.ptsDeltaInCaseOfDiscontinuity();
                    discontinuity = true;
                }
            }

    #if 0
            std::cout<<"pts "<<(pts & PTS_MASK)<<" ("<<pts<<"), effectivePts "<<(effectivePts & PTS_MASK)<<" ("<< effectivePts <<")";
            if( nonMonotonic )
                std::cout<<", nonMonotonic";
            if( discontinuity )
                std::cout << ", discontinuity";
            std::cout<<std::endl;
    #endif

            const auto timestamp = ptsToClockMapper.getTimestamp( effectivePts & PTS_MASK );
            if( firstTimestamp == -1 )
                firstTimestamp = timestamp;
            const auto tsDiff = timestamp - firstTimestamp;
            //const auto ptsDiff = effectivePts - MIN_PTS;
            const auto ptsDiff = ptsDelta - (pts - effectivePts);
            ASSERT_EQ( tsDiff, ptsDiff * PTS_FREQUENCY );

#if 1
            if( rand() % 50 == 0 )
                ptsToClockMapper.updateTimeMapping( effectivePts & PTS_MASK, timestamp );
#endif

            prevEffectivePts = effectivePts;
            pts += STEP_VALUE;
            ptsDelta += STEP_VALUE;

            prevTimestamp = timestamp;
        }
    }
}
