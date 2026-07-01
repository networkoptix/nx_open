// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include <QtCore/QString>

#include <nx/utils/cached_response.h>
#include <nx/utils/lockable.h>
#include <nx/utils/time.h>

namespace {

class Request
{
public:

    Request(std::optional<std::chrono::milliseconds> delay = std::nullopt) : m_delay(delay) {}
    Request(const Request&) = delete;
    Request(Request&&) = default;

    std::optional<std::pair<int, int>> operator()(int param)
    {
        if(m_delay)
            std::this_thread::sleep_for(*m_delay);

        auto callCounts = m_callCounts.lock();
        return std::pair<int, int>(param, ++(*callCounts)[param]);
    }

private:
    nx::Lockable<std::unordered_map</*param*/ int, /*count*/ int>> m_callCounts;
    const std::optional<std::chrono::milliseconds> m_delay;
};

const std::chrono::milliseconds kProtectiveShift{ 1 };
const nx::CachedResponseTimeouts kDefaultTimeouts;

struct TrackedValue
{
    static int s_copies;
    static void reset() { s_copies = 0; }

    int value = 0;

    TrackedValue() = default;
    explicit TrackedValue(int v) : value(v) {}
    TrackedValue(const TrackedValue& o) noexcept : value(o.value) { ++s_copies; }
    TrackedValue(TrackedValue&& o) = default;
    TrackedValue& operator=(const TrackedValue& o) noexcept { value = o.value; ++s_copies; return *this; }
    TrackedValue& operator=(TrackedValue&& o) = default;
};
int TrackedValue::s_copies = 0;

template<typename Invoke>
void checkNoCopies(Invoke invoke)
{
    TrackedValue::reset();
    auto response = invoke();
    EXPECT_EQ(TrackedValue::s_copies, 0);
    response = invoke();
    EXPECT_EQ(TrackedValue::s_copies, 0);
}

} // namespace

namespace nx::test {

TEST(CachedResponse, SimpleGet)
{
    CachedResponse<std::optional<int>()> goodResponse([n = 0]() mutable { return ++n; });

    ASSERT_TRUE(goodResponse());
    ASSERT_EQ(*goodResponse(), 1);

    CachedResponse<std::optional<int>()> badResponse(
        []() -> std::optional<int> { return std::nullopt; });

    ASSERT_FALSE(badResponse());
}

TEST(CachedResponse, Invalidate)
{
    CachedResponse<std::optional<int>()> response([n = 0]() mutable { return ++n; });

    ASSERT_TRUE(response());
    ASSERT_EQ(*response(), 1);

    response.invalidate();

    ASSERT_TRUE(response());
    ASSERT_EQ(*response(), 2);
}

TEST(CachedResponse, UpdateAfterExpired)
{
    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    CachedResponse<std::optional<int>()> response([n = 0]() mutable { return ++n; });

    ASSERT_TRUE(response());
    ASSERT_EQ(*response(), 1);

    timeShift.applyRelativeShift(kDefaultTimeouts.updateTimeout + kProtectiveShift);

    ASSERT_TRUE(response());
    ASSERT_EQ(*response(), 2);
}

TEST(CachedResponse, UpdateAfterFail)
{
    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    CachedResponse<std::optional<int>()> response(
        [n = 0]() mutable -> std::optional<int>
        {
            if (++n % 2 == 0)
                return std::nullopt;
            return n;
        });

    ASSERT_TRUE(response());
    ASSERT_EQ(*response(), 1);

    timeShift.applyRelativeShift(kDefaultTimeouts.updateTimeout + kProtectiveShift);

    ASSERT_FALSE(response());

    timeShift.applyRelativeShift(kDefaultTimeouts.failedUpdateTimeout + kProtectiveShift);

    ASSERT_TRUE(response());
    ASSERT_EQ(*response(), 3);
}

TEST(CachedResponse, Concurrency)
{
    CachedResponse<std::optional<int>()> response(
        [n = 0]() mutable
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            return ++n;
        });

    std::array<std::thread, 32> threads;
    for (int i = 0; i < (int) threads.size(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        threads[i] = std::thread(
            [i, &response]()
            {
                const auto result = response();
                EXPECT_TRUE(result) << "thread " << i;
                EXPECT_EQ(*result, 1) << "thread " << i;
            });
    }

    for (auto& t: threads)
        t.join();
}

TEST(CachedResponse, Timeouts)
{
    ASSERT_GE(
        kDefaultTimeouts.updateTimeout,
        kDefaultTimeouts.failedUpdateTimeout);

    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);
    CachedResponseTimeouts notDefaultTimeouts{.updateTimeout = std::chrono::seconds{1}};

    Request request;
    CachedResponse<std::optional<std::pair<int, int>>()> response(
        [&request]() { return request(0); },
        notDefaultTimeouts);

    auto getExpectedResult = [](int v1, int v2) { return std::pair(v1, v2); };

    for (int i = 0; i < 3; i++)
    {
        const auto res = response();
        ASSERT_TRUE(res);
        ASSERT_EQ(*res, getExpectedResult(0, i + 1));
        timeShift.applyRelativeShift(notDefaultTimeouts.updateTimeout + kProtectiveShift);
    }
}

TEST(CachedResponse, CallWithArguments)
{
    CachedResponse<std::optional<int>(int, int)> response([](int a, int b) { return a + b; });

    const auto result = response(2, 3);
    ASSERT_TRUE(result);
    ASSERT_EQ(*result, 5);
}

TEST(CachedResponse, ArgumentsDoNotAffectCachedValueBeforeExpiration)
{
    CachedResponse<std::optional<int>(int)> response([](int x) {  return x * 10; });

    const auto first = response(2);
    ASSERT_TRUE(first);
    ASSERT_EQ(*first, 20);

    const auto second = response(7);
    ASSERT_TRUE(second);
    ASSERT_EQ(*second, 20);
}

TEST(CachedResponse, ArgumentsAffectValueAfterExpirationOrInvalidate)
{
    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    CachedResponse<std::optional<int>(int)> response([](int x) {  return x * 10; });

    const auto first = response(2);
    ASSERT_TRUE(first);
    ASSERT_EQ(*first, 20);

    timeShift.applyRelativeShift(kDefaultTimeouts.updateTimeout + kProtectiveShift);

    const auto second = response(7);
    ASSERT_TRUE(second);
    ASSERT_EQ(*second, 70);

    response.invalidate();

    const auto third = response(2);
    ASSERT_TRUE(third);
    ASSERT_EQ(*third, 20);
}

TEST(CachedResponse, SupportsMoveOnlyArgument)
{
    CachedResponse<std::optional<int>(std::unique_ptr<int>)> response(
        [](std::unique_ptr<int> value)
        {
            return *value + 1;
        });

    auto result = response(std::make_unique<int>(41));
    ASSERT_TRUE(result);
    ASSERT_EQ(*result, 42);
}

TEST(CachedResponse, SupportsStringArgument)
{
    CachedResponse< std::optional<size_t>(std::string)> response(
        [](const std::string& s)
        {
            return s.size();
        });

    const auto result = response("hello");
    ASSERT_TRUE(result);
    ASSERT_EQ(*result, 5u);
}

TEST(CachedResponse, SupportsClassStaticMethods)
{
    struct ClassWithLongOperations
    {
    private:
        static std::optional<int> loadResult()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
            return 0;
        }

    public:
        CachedResponse<std::optional<int>()> result;
        ClassWithLongOperations() : result{ &ClassWithLongOperations::loadResult } {}
    };

    ClassWithLongOperations response;
    const auto result = response.result();

    ASSERT_TRUE(result);
    ASSERT_EQ(*result, 0);
}

TEST(ParameterizedCachedResponse, SimpleGet)
{
    auto getter = [](int i) -> std::optional<int>
        {
            if (i == 0)
                return std::nullopt;
            return i;
        };

    auto keyGenerator = [](int i) { return i; };

    ParameterizedCachedResponse<int, std::optional<int>(int)> goodResponse(
        getter,
        keyGenerator);

    ASSERT_FALSE(goodResponse(0));

    ASSERT_TRUE(goodResponse(1));
    ASSERT_EQ(*goodResponse(1), 1);
}

TEST(ParameterizedCachedResponse, RvalueArgumentsNotConsumedByKeyGenerator)
{
    auto getter = [](const std::string& s) -> std::optional<size_t> { return s.size(); };
    auto keyGenerator = [](const std::string& s) { return s; };

    ParameterizedCachedResponse<std::string, std::optional<size_t>(std::string)> response(
        getter,
        keyGenerator);

    auto result = response(std::string("hello"));
    ASSERT_TRUE(result);
    ASSERT_EQ(*result, 5u);
}

TEST(ParameterizedCachedResponse, MoreComplexCases)
{
    auto getter = [](QString s1, const std::string& s2, int i) -> std::optional<QString>
        {
            return s1 + QString::fromStdString(s2) + QString::number(i);
        };

    auto keyGenerator = [](QString s1, std::string s2, int i)
        {
            return std::make_tuple(std::move(s1), std::move(s2), i);
        };

    using KeyType = std::tuple<QString, std::string, int>;
    ParameterizedCachedResponse<KeyType, std::optional<QString>(QString, std::string, int)> response(
        getter,
        keyGenerator);

    std::string s = "string";
    ASSERT_TRUE(response("QString", s, 1));
    ASSERT_EQ(*response("QString", s, 1), "QStringstring1");
}

TEST(ParameterizedCachedResponse, GetterStateIsSharedAcrossKeys)
{
    int totalCalls = 0;
    auto keyGenerator = [](int param) { return param; };

    ParameterizedCachedResponse<int, std::optional<int>(int)> response(
        [&totalCalls](int param) -> std::optional<int>
        {
            ++totalCalls;
            return param * 10;
        },
        keyGenerator);

    response(1);
    response(2);
    response(3);
    ASSERT_EQ(totalCalls, 3);

    response(1);
    response(2);
    ASSERT_EQ(totalCalls, 3);
}

TEST(ParameterizedCachedResponse, Invalidate)
{
    auto keyGenerator = [](int param) { return param; };

    ParameterizedCachedResponse<int, std::optional<std::pair<int, int>>(int)> response(
        Request{},
        keyGenerator);

    auto getExpectedResult = [](int v1, int v2) { return std::pair<int, int>(v1, v2); };

    ASSERT_TRUE(response(0));
    ASSERT_EQ(*response(0), getExpectedResult(0, 1));
    ASSERT_TRUE(response(1));
    ASSERT_EQ(*response(1), getExpectedResult(1, 1));

    response.invalidate(0);
    ASSERT_TRUE(response(0));
    ASSERT_EQ(*response(0), getExpectedResult(0, 2)); //<func(0) should be called twice
    ASSERT_TRUE(response(1));
    ASSERT_EQ(*response(1), getExpectedResult(1, 1)); //<func(1) should be called once

    response.invalidate(); //< invalidate all
    ASSERT_TRUE(response(0));
    ASSERT_EQ(*response(0), getExpectedResult(0, 3));
    ASSERT_TRUE(response(1));
    ASSERT_EQ(*response(1), getExpectedResult(1, 2));
}

TEST(ParameterizedCachedResponse, Timeouts)
{
    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);
    CachedResponseTimeouts notDefaultTimeouts{ .updateTimeout = std::chrono::seconds{1} };

    auto keyGenerator = [](int param) { return param; };

    ParameterizedCachedResponse<int, std::optional<std::pair<int, int>>(int)> response(
        Request{},
        keyGenerator,
        notDefaultTimeouts);

    auto getExpectedResult = [](int v1, int v2) { return std::pair<int, int>(v1, v2); };

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            const auto res = response(j);
            ASSERT_TRUE(res);
            ASSERT_EQ(*res, getExpectedResult(j, i + 1));
        }

        timeShift.applyRelativeShift(notDefaultTimeouts.updateTimeout + kProtectiveShift);
    }
}

TEST(ParameterizedCachedResponse, Concurrency)
{
    const auto delay = std::chrono::milliseconds(500);
    const int parametersCount = 4;

    auto keyGenerator = [](int param) { return param; };

    ParameterizedCachedResponse<int, std::optional<std::pair<int, int>>(int)> response(
        Request{ delay },
        keyGenerator);

    auto start = std::chrono::high_resolution_clock::now();

    std::array<std::thread, 32> threads;
    for (int i = 0; i < (int)threads.size(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        threads[i] = std::thread(
            [i, &response]()
            {
                // Expect that Request::operator() was called only once for each param
                auto getExpectedResult = [](int v) { return std::pair<int, int>(v, 1); };

                const int param = i % parametersCount;
                const auto result = response(param);
                EXPECT_TRUE(result) << "thread " << i;
                EXPECT_EQ(*result, getExpectedResult(param)) << "thread " << i;
            });
    }

    for (auto& t: threads)
        t.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Verify that long-running computation functions with different parameters can execute in parallel.
    ASSERT_TRUE(duration < 2 * delay);
}

TEST(CachedResponse, CheckCopyCount)
{
    TrackedValue session{42};
    {
        TrackedValue::reset();
        CachedResponse<std::optional<int>(TrackedValue)> cache(
            [](TrackedValue s) -> std::optional<int> { return s.value; });
        const auto response = cache(session);
        ASSERT_TRUE(response);
        EXPECT_EQ(TrackedValue::s_copies, 1);
    }
    {
        CachedResponse<std::optional<int>(TrackedValue*)> cache(
            [](TrackedValue* s) -> std::optional<int> { return s->value; });
        checkNoCopies([&]{ return cache(&session); });
    }
    {
        CachedResponse<std::optional<int>(const TrackedValue&)> cache(
            [](const TrackedValue& s) -> std::optional<int> { return s.value; });
        checkNoCopies([&]{ return cache(session); });
    }
}

TEST(ParameterizedCachedResponse, CheckCopyCount)
{
    TrackedValue session{42};
    {
        TrackedValue::reset();
        ParameterizedCachedResponse<int, std::optional<int>(TrackedValue)> cache(
            [](TrackedValue s) -> std::optional<int> { return s.value; },
            [](TrackedValue s) { return s.value; });
        const auto response = cache(session);
        ASSERT_TRUE(response);
        EXPECT_EQ(TrackedValue::s_copies, 2); // keyGenerator + getter
    }
    {
        ParameterizedCachedResponse<int, std::optional<int>(TrackedValue*)> cache(
            [](TrackedValue* s) -> std::optional<int> { return s->value; },
            [](TrackedValue* s) { return s->value; });
        checkNoCopies([&]{ return cache(&session); });
    }
    {
        ParameterizedCachedResponse<int, std::optional<int>(const TrackedValue&)> cache(
            [](const TrackedValue& s) -> std::optional<int> { return s.value; },
            [](const TrackedValue& s) { return s.value; });
        checkNoCopies([&]{ return cache(session); });
    }
}

} // namespace nx::test
