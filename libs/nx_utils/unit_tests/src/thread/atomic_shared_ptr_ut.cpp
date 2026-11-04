// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <nx/utils/thread/atomic_shared_ptr.h>

namespace nx::utils::test {

TEST(AtomicSharedPtr, exchange_returns_the_value_it_replaces)
{
    AtomicSharedPtr<int> value(std::make_shared<int>(1));
    ASSERT_EQ(1, *value.load());

    const auto previous = value.exchange(std::make_shared<int>(2));
    ASSERT_EQ(1, *previous);
    ASSERT_EQ(2, *value.load());

    value.store(nullptr);
    ASSERT_EQ(nullptr, value.load());
}

TEST(AtomicSharedPtr, compare_exchange_installs_only_over_the_expected_value)
{
    const auto first = std::make_shared<int>(1);
    AtomicSharedPtr<int> value(first);

    auto stale = std::make_shared<int>(1);
    ASSERT_FALSE(value.compare_exchange_strong(stale, std::make_shared<int>(2)));
    ASSERT_EQ(first, stale);
    ASSERT_EQ(first, value.load());

    ASSERT_TRUE(value.compare_exchange_strong(stale, std::make_shared<int>(3)));
    ASSERT_EQ(3, *value.load());
}

TEST(AtomicSharedPtr, compare_exchange_requires_shared_ownership)
{
    AtomicSharedPtr<int> value(std::make_shared<int>(1));

    // The same pointer, not owning it.
    std::shared_ptr<int> alias(std::shared_ptr<int>(), value.load().get());
    ASSERT_FALSE(value.compare_exchange_strong(alias, std::make_shared<int>(2)));
    ASSERT_EQ(1, *value.load());
    ASSERT_EQ(value.load(), alias);
}

TEST(AtomicSharedPtr, concurrent_weak_exchange_loops_lose_no_change)
{
    constexpr int kThreads = 8;
    constexpr int kChangesPerThread = 1000;

    AtomicSharedPtr<const int> counter(std::make_shared<const int>(0));
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i)
    {
        threads.emplace_back(
            [&counter]()
            {
                for (int j = 0; j < kChangesPerThread; ++j)
                {
                    auto current = counter.load();
                    while (!counter.compare_exchange_weak(
                        current, std::make_shared<const int>(*current + 1)))
                    {
                    }
                }
            });
    }
    for (auto& thread: threads)
        thread.join();

    ASSERT_EQ(kThreads * kChangesPerThread, *counter.load());
}

namespace {

/** Reads the pointer that held it when destroyed, which the mutex would block. */
struct ReadsOwnerOnDestruction
{
    AtomicSharedPtr<ReadsOwnerOnDestruction>* owner = nullptr;
    bool* destroyed = nullptr;

    ~ReadsOwnerOnDestruction()
    {
        owner->load();
        *destroyed = true;
    }
};

} // namespace

TEST(AtomicSharedPtr, replaced_value_is_destroyed_outside_the_mutex)
{
    AtomicSharedPtr<ReadsOwnerOnDestruction> value;
    bool destroyed = false;

    value.store(std::make_shared<ReadsOwnerOnDestruction>(&value, &destroyed));
    value.store(nullptr);
    ASSERT_TRUE(destroyed);

    // A failed exchange replaces the expected, which may hold the last reference to its value.
    destroyed = false;
    value.store(std::make_shared<ReadsOwnerOnDestruction>(&value, &destroyed));
    auto expected = value.load();
    value.store(nullptr);
    ASSERT_FALSE(destroyed);
    ASSERT_FALSE(value.compare_exchange_strong(expected, nullptr));
    ASSERT_TRUE(destroyed);
}

} // namespace nx::utils::test
