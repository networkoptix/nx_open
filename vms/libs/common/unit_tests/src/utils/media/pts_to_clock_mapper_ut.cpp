/**********************************************************
* Jul 23, 2015
* a.kolesnikov
***********************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <utils/media/pts_to_clock_mapper.h>
#include <nx/utils/random.h>

typedef uint64_t pts_type;
typedef PtsToClockMapper::ts_type ts_type;

TEST( PtsToClockMapper, general )
{
    typedef PtsToClockMapper MapperType;

    const int PTS_FREQUENCY = 1000;
    const size_t ptsBitsToTest[] = { 32, 20, 12 };

    for( const size_t PTS_BITS: ptsBitsToTest )
    {
        const size_t PTS_MASK = (1LL<<PTS_BITS) -1;

        for( int j = 0; j < 100; ++j )
        {
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

            ptsToClockMapper.updateTimeMapping( MIN_PTS, nx::utils::random::number() );

            //for( pts_type pts = MIN_PTS; pts < MAX_PTS; pts += STEP_VALUE )
            for( pts_type i = 0; i < STEPS; ++i )
            {
                auto effectivePts = pts;

                bool nonMonotonic = false;
                bool discontinuity = false;
                if( i > 0 )
                {
                    if( nx::utils::random::number(0, 6) == 0 )
                    {
                        //emulating non-monotonic pts 
                        effectivePts -= STEP_VALUE + STEP_VALUE/2;
                        nonMonotonic = true;
                    }
                    else if( nx::utils::random::number(0, 14) == 0 )
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
                if( nx::utils::random::number(0, 50) == 0 )
                    ptsToClockMapper.updateTimeMapping( effectivePts & PTS_MASK, timestamp );
#endif

                prevEffectivePts = effectivePts;
                pts += STEP_VALUE;
                ptsDelta += STEP_VALUE;

                prevTimestamp = timestamp;
            }
        }
    }
}
