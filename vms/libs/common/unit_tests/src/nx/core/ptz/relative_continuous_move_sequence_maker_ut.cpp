#include <gtest/gtest.h>

#include <vector>
#include <chrono>

#include <nx/core/ptz/relative/relative_continuous_move_sequence_maker.h>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace nx {
namespace core {
namespace ptz {

namespace {

struct TestCase
{
    std::vector<Speed> mapping;
    Vector inputVector;
    CommandSequence resultSequence;
};

static const std::vector<Speed> kEqualSpeedMapping = {
    {1.0, 10000ms},
    {1.0, 10000ms},
    {1.0, 10000ms},
    {1.0, 10000ms}
};

static const std::vector<TestCase> kTestCases = {
    {
        kEqualSpeedMapping,
        Vector(1.0, 1.0, 1.0, 1.0, 0.0),
        {
            {Vector(1.0, 1.0, 1.0, 1.0, 0.0), Options(), 10000ms}
        }
    },
    {
        kEqualSpeedMapping,
        Vector(0.1, 0.2, 0.3, 0.4),
        {
            {Vector(1.0, 1.0, 1.0, 1.0, 0.0), Options(), 1000ms},
            {Vector(0.0, 1.0, 1.0, 1.0, 0.0), Options(), 1000ms},
            {Vector(0.0, 0.0, 1.0, 1.0, 0.0), Options(), 1000ms},
            {Vector(0.0, 0.0, 0.0, 1.0, 0.0), Options(), 1000ms}
        }
    },
    {
        {
            {1.0, 10000ms},
            {1.0, 8000ms},
            {1.0, 6000ms},
            {1.0, 4000ms}
        },
        Vector(1.0, 1.0, 1.0, 1.0, 0.0),
        {
            {Vector(1.0, 1.0, 1.0, 1.0, 0.0), Options(), 4000ms},
            {Vector(1.0, 1.0, 1.0, 0.0, 0.0), Options(), 2000ms},
            {Vector(1.0, 1.0, 0.0, 0.0, 0.0), Options(), 2000ms},
            {Vector(1.0, 0.0, 0.0, 0.0, 0.0), Options(), 2000ms}
        }
    },
    {
        {
            { 1.0, 10000ms },
            { 0.8, 8000ms },
            { 0.6, 6000ms },
            { 0.4, 4000ms }
        },
        Vector(1.0, 1.0, 1.0, 1.0, 0.0),
        {
            {Vector(0.0, 0.0, 0.0, 0.4, 0.0), Options(), 4000ms},
            {Vector(0.0, 0.0, 0.6, 0.0, 0.0), Options(), 6000ms},
            {Vector(0.0, 0.8, 0.0, 0.0, 0.0), Options(), 8000ms},
            {Vector(1.0, 0.0, 0.0, 0.0, 0.0), Options(), 10000ms}
        }
    },
    {
        {
            {1.0, 10000ms},
            {1.0, 10000ms},
            {0.5, 10000ms},
            {0.5, 10000ms}
        },
        Vector(1.0, 1.0, 1.0, 1.0, 0.0),
        {
            {Vector(0.0, 0.0, 0.5, 0.5, 0.0), Options(), 10000ms},
            {Vector(1.0, 1.0, 0.0, 0.0, 0.0), Options(), 10000ms}
        }
    },
    {
        {
            {1.0, 10000ms},
            {0.8, 10000ms},
            {0.6, 10000ms},
            {0.4, 10000ms}
        },
        Vector(1.0, 1.0, 1.0, 1.0, 0.0),
        {
            {Vector(0.0, 0.0, 0.0, 0.4, 0.0), Options(), 10000ms},
            {Vector(0.0, 0.0, 0.6, 0.0, 0.0), Options(), 10000ms},
            {Vector(0.0, 0.8, 0.0, 0.0, 0.0), Options(), 10000ms},
            {Vector(1.0, 0.0, 0.0, 0.0, 0.0), Options(), 10000ms}
        }
    }
};

bool isRelativeVectorEqualToContinuousSequence(
    const Vector& relativeMovementVector,
    const CommandSequence& continuousMoveSequence,
    const RelativeContinuousMoveMapping& mapping)
{
    Vector relativeNormalized;
    Vector continuousNormalizedSum;
    for (const auto component: kAllComponents)
    {
        for (const auto& command: continuousMoveSequence)
        {
            Vector continuousNormalized;
            continuousNormalized.setComponent(
                command.command.component(component) * command.duration.count(),
                component);

            continuousNormalizedSum += continuousNormalized;
        }

        const auto componentMapping = mapping.componentMapping(component);
        relativeNormalized.setComponent(
            relativeMovementVector.component(component)
                * componentMapping.workingSpeed.absoluteValue
                * componentMapping.workingSpeed.cycleDuration.count(),
            component);
    }

    return qFuzzyEquals(relativeNormalized, continuousNormalizedSum);
}

} // namespace

TEST(SequenceMaker, makingSequence)
{
    for (const auto& testCase: kTestCases)
    {
        RelativeContinuousMoveMapping mapping(testCase.mapping);
        RelativeContinuousMoveSequenceMaker sequenceMaker(mapping);

        const auto sequence = sequenceMaker.makeSequence(
            testCase.inputVector,
            Options());

        ASSERT_TRUE(
            isRelativeVectorEqualToContinuousSequence(
                testCase.inputVector,
                testCase.resultSequence,
                testCase.mapping));
    }
}

} // namespace ptz
} // namespace core
} // namespace nx
