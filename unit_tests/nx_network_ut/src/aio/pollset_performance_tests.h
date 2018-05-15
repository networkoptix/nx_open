#pragma once

#include <gtest/gtest.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

template<typename T>
class PollSetPerformance:
    public ::testing::Test
{
protected:
    struct TestResult
    {
        std::chrono::milliseconds runDuration;
        int iterationsDone;

        TestResult():
            runDuration(std::chrono::milliseconds::zero()),
            iterationsDone(0)
        {
        }
    };

    TestResult calculateAddRemovePerformance()
    {
        using namespace std::chrono;

        constexpr milliseconds testDurationLimit = seconds(1);

        T pollset;
        TCPSocket tcpSocket(AF_INET);

        const auto startTime = std::chrono::steady_clock::now();
        TestResult testResult;
        for (;
            std::chrono::steady_clock::now() < startTime + testDurationLimit;
            ++testResult.iterationsDone)
        {
            NX_GTEST_ASSERT_TRUE(pollset.add(&tcpSocket, aio::etRead));
            pollset.remove(&tcpSocket, aio::etRead);
        }
        testResult.runDuration =
            duration_cast<milliseconds>(std::chrono::steady_clock::now() - startTime);

        return testResult;
    }

    void printResult(const TestResult& testResult)
    {
        std::cout << "add/remove performance. "
            "Total " << testResult.iterationsDone << " calls made in "
            << testResult.runDuration.count() << " ms. That gives "
            << (testResult.iterationsDone * 1000 / testResult.runDuration.count())
            << " calls per second"
            << std::endl;
    }
};

TYPED_TEST_CASE_P(PollSetPerformance);

TYPED_TEST_P(PollSetPerformance, add_remove)
{
    const auto result = this->calculateAddRemovePerformance();
    this->printResult(result);
}

REGISTER_TYPED_TEST_CASE_P(PollSetPerformance, add_remove);

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
