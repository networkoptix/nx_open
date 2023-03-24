// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <deque>

#include <gtest/gtest.h>

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/coro/task_utils.h>
#include <nx/utils/coro/when_all.h>

namespace nx::coro::test {

class TaskTest: public ::testing::Test
{
protected:
    TaskTest()
    {
    }

    ~TaskTest()
    {
    }

    using Messages = std::vector<std::string>;
    using Func = std::function<void(std::string)>;

    struct DelayedCallback
    {
        Func callback;
        std::string arg;
    };

    std::deque<DelayedCallback> delayedCallbacks;

    void asyncCall(const std::string& arg, Func callback)
    {
        delayedCallbacks.push_back({.callback = std::move(callback), .arg = arg});
    }

    void triggerAsyncCall()
    {
        if (delayedCallbacks.empty())
            return;

        auto current = *delayedCallbacks.begin();
        delayedCallbacks.pop_front();
        current.callback(current.arg);
    }

    void dropAsyncCall()
    {
        if (delayedCallbacks.empty())
            return;

        auto current = *delayedCallbacks.begin();
        delayedCallbacks.pop_front();
    }

    auto wrapAsyncCall(const std::string& arg)
    {
        // Return custom suspend_always.
        struct suspend_always
        {
            suspend_always(const std::string& arg, TaskTest* test)
                :
                m_test(test),
                m_arg(arg)
            {
                m_test->m_messages.emplace_back("wrap constructed with " + m_arg);
            }

            bool await_ready() const { return false; }

            void await_suspend(std::coroutine_handle<> h)
            {
                m_test->m_messages.emplace_back("suspend " + m_arg);
                m_test->asyncCall(
                    m_arg,
                    [h = std::move(h), this](const std::string& arg) mutable
                    {
                        m_test->m_messages.emplace_back("callback " + arg);
                        m_result = "result from " + arg;
                        if (isCancelRequested(h))
                            m_canceled = true;
                        h();
                    });
            }

            std::string await_resume() const
            {
                if (m_canceled || m_arg == "cancel")
                    throw TaskCancelException();

                return m_result;
            }

            ~suspend_always()
            {
                m_test->m_messages.emplace_back("wrap destroyed with " + m_arg);
            }

            TaskTest* m_test;
            bool m_canceled = false;
            std::string m_arg;
            std::string m_result;
        };

        return suspend_always{arg, this};
    }

    Messages m_messages;

    struct Scope
    {
        Scope(const std::string& name, TaskTest* test)
            :
            m_name(name),
            m_test(test)
        {
            m_test->m_messages.push_back("scope begin " + m_name);
        }

        ~Scope()
        {
            m_test->m_messages.push_back("scope end " + m_name);
        }

        std::string m_name;
        TaskTest* m_test;
    };
};

TEST_F(TaskTest, simpleContinuation)
{
    int result = 0;

    const auto getOne =
        []() -> Task<int>
        {
            co_return 1;
        };

    const auto test =
        [&]() -> Task<>
        {
            result = co_await getOne();
        };

    auto t = test();
    t.start();

    EXPECT_EQ(1, result);
    EXPECT_TRUE(t.done());
}

TEST_F(TaskTest, taskIsOwningHandle)
{
    {
        const auto test =
            [&]() -> Task<>
            {
                Scope s{"test", this};

                m_messages.push_back(co_await wrapAsyncCall("A"));
            };

        auto task = test();

        task.start();

    } //< Coroutine is destroyed because 'task' goes out of scope.

    static const Messages kFull{
        "scope begin test",
        "wrap constructed with A",
        "suspend A",
        "wrap destroyed with A",
        "scope end test",
    };

    ASSERT_EQ(kFull, m_messages);
}

TEST_F(TaskTest, multipleAwait)
{
    const auto foo =
        [&](const std::string a) -> Task<std::string>
        {
            Scope s{"foo", this};

            std::string val = co_await wrapAsyncCall(a);
            std::string val2 = co_await wrapAsyncCall("B");

            co_return val + "+" + val2;
        };

    const auto test =
        [&]() -> Task<>
        {
            Scope s{"test", this};

            m_messages.push_back(co_await foo("A"));
        };

    auto t = test();

    ASSERT_TRUE(m_messages.empty());

    t.start();

    static const Messages kBeforeA{
        "scope begin test",
        "scope begin foo",
        "wrap constructed with A",
        "suspend A",
    };
    ASSERT_EQ(kBeforeA, m_messages);

    triggerAsyncCall(); //< callback A

    static const Messages kAfterB{
        "scope begin test",
        "scope begin foo",
        "wrap constructed with A",
        "suspend A",
        "callback A",
        "wrap destroyed with A",
        "wrap constructed with B",
        "suspend B",
    };
    ASSERT_EQ(kAfterB, m_messages);

    triggerAsyncCall(); //< callback B

    static const Messages kFull{
        "scope begin test",
        "scope begin foo",
        "wrap constructed with A",
        "suspend A",
        "callback A",
        "wrap destroyed with A",
        "wrap constructed with B",
        "suspend B",
        "callback B",
        "wrap destroyed with B",
        "scope end foo",
        "result from A+result from B",
        "scope end test",
    };
    ASSERT_EQ(kFull, m_messages);

    EXPECT_TRUE(t.done());
}

TEST_F(TaskTest, unfinishedCoroutineDestroyed)
{
    const auto foo =
        [&](const std::string a) -> Task<std::string>
        {
            Scope s{"foo", this};

            std::string val = co_await wrapAsyncCall(a);
            std::string val2 = co_await wrapAsyncCall("B");

            co_return val + "+" + val2;
        };

    const auto test =
        [&]() -> Task<>
        {
            Scope s{"test", this};
            m_messages.push_back(co_await foo("A"));
        };

    {
        auto t = test();
        t.start();

        static const Messages kBeforeA{
            "scope begin test",
            "scope begin foo",
            "wrap constructed with A",
            "suspend A",
        };
        ASSERT_EQ(kBeforeA, m_messages);

        triggerAsyncCall(); //< callback A

        static const Messages kAfterA{
            "scope begin test",
            "scope begin foo",
            "wrap constructed with A",
            "suspend A",
            "callback A",
            "wrap destroyed with A",
            "wrap constructed with B",
            "suspend B",
        };
        ASSERT_EQ(kAfterA, m_messages);

        EXPECT_FALSE(t.done());
    } //<  No callback B, but scopes should be destroyed because t is destroyed.

    static const Messages kFull{
        "scope begin test",
        "scope begin foo",
        "wrap constructed with A",
        "suspend A",
        "callback A",
        "wrap destroyed with A",
        "wrap constructed with B",
        "suspend B",
        "wrap destroyed with B",
        "scope end foo",
        "scope end test",
    };
    ASSERT_EQ(kFull, m_messages);
}

TEST_F(TaskTest, rethrowException)
{
    const auto foo =
        [&](const std::string a) -> Task<std::string>
        {
            Scope s{"foo", this};

            std::string val = co_await wrapAsyncCall(a);
            if (!val.empty())
                throw std::invalid_argument("exception with " + val);

            std::string val2 = co_await wrapAsyncCall("B");

            co_return val + "+" + val2;
        };

    const auto test =
        [&]() -> Task<>
        {
            Scope s{"test", this};
            m_messages.push_back(co_await foo("A"));
        };

    {
        auto t = test();
        t.start();

        static const Messages kBeforeA{
            "scope begin test",
            "scope begin foo",
            "wrap constructed with A",
            "suspend A",
        };
        ASSERT_EQ(kBeforeA, m_messages);

        ASSERT_DEATH(triggerAsyncCall(), "");

        EXPECT_FALSE(t.done());
    } //< Scopes should be destroyed because 't' goes out of scope.

    static const Messages kFull{
        "scope begin test",
        "scope begin foo",
        "wrap constructed with A",
        "suspend A",
        "wrap destroyed with A",
        "scope end foo",
        "scope end test",
    };
    ASSERT_EQ(kFull, m_messages);
}

TEST_F(TaskTest, catchExceptionFromCoAwait)
{
    const auto foo =
        [&](const std::string a) -> Task<std::string>
        {
            Scope s{"foo", this};

            throw std::invalid_argument("exception");

            co_return co_await wrapAsyncCall(a);
        };

    const auto test =
        [&]() -> Task<>
        {
            Scope s{"test", this};

            try
            {
                m_messages.push_back(co_await foo("A"));
            }
            catch (std::invalid_argument& e)
            {
                m_messages.push_back(e.what());
            }
        };

    auto t = test();

    t.start();

    static const Messages kBeforeTaskDestructor{
        "scope begin test",
        "scope begin foo",
        "scope end foo",
        "exception",
        "scope end test",
    };
    ASSERT_EQ(kBeforeTaskDestructor, m_messages);

    // Scopes should be destroyed because of exception.
    EXPECT_TRUE(t.done());
}

TEST_F(TaskTest, fireAndForget)
{
    {
        const auto test =
            [this](std::string msg) -> FireAndForget
            {
                // Copy captures into coroutine stack because 'test' captures are destroyed when
                // 'test' goes out of scope.
                auto self = this;

                Scope s{"test", self};

                self->m_messages.push_back(co_await self->wrapAsyncCall("a"));
                self->m_messages.push_back(msg);
            };

        test("b");
    } //< 'test' lambda captures are destroyed here.

    triggerAsyncCall();

    static const Messages kFull{
        "scope begin test",
        "wrap constructed with a",
        "suspend a",
        "callback a",
        "result from a",
        "wrap destroyed with a",
        "b",
        "scope end test"
    };
    ASSERT_EQ(kFull, m_messages);
}

TEST_F(TaskTest, cancelException)
{
    const auto foo =
        [this](const std::string str) -> Task<std::string>
        {
            Scope s{"foo", this};

            m_messages.push_back(co_await wrapAsyncCall(str));

            co_return "unreachable";
        };

    {
        const auto test =
            [this, &foo]() -> FireAndForget
            {
                // Copy captures into coroutine stack because 'test' captures are destroyed when
                // 'test' goes out of scope.
                auto self = this;
                auto& fooRef = foo;

                Scope s{"test", this};

                self->m_messages.push_back(co_await self->wrapAsyncCall("a"));
                self->m_messages.push_back(co_await fooRef("cancel"));
                self->m_messages.push_back("unreachable");
            };

        test();
    } //< 'test' lambda captures are destroyed here.

    triggerAsyncCall();
    triggerAsyncCall();

    static const Messages kFull{
        "scope begin test",
        "wrap constructed with a",
        "suspend a",
        "callback a",
        "result from a",
        "wrap destroyed with a",
        "scope begin foo",
        "wrap constructed with cancel",
        "suspend cancel",
        "callback cancel",
        "wrap destroyed with cancel",
        "scope end foo",
        "scope end test",
    };
    ASSERT_EQ(kFull, m_messages);
}

TEST_F(TaskTest, QObjectGuard)
{
    QObject* destructible = new QObject();

    const auto foo =
        [this](const std::string str) -> Task<std::string>
        {
            Scope s{"foo", this};

            m_messages.push_back(co_await wrapAsyncCall(str));

            co_return "foo result";
        };

    const auto test =
        [this, destructible, &foo](std::string msg) -> FireAndForget
        {
            co_await guard(destructible);

            // Copy captures into coroutine stack because 'test' captures are destroyed when
            // 'test' goes out of scope.
            auto self = this;

            Scope s{"test", self};

            self->m_messages.push_back(co_await foo("a"));
            self->m_messages.push_back(co_await self->wrapAsyncCall("b"));
            self->m_messages.push_back(co_await self->wrapAsyncCall("c"));

            self->m_messages.push_back(msg);
        };

    ASSERT_TRUE(m_messages.empty());

    test("d");

    static const Messages kBeforeFirstSuspend{
        "scope begin test",
        "scope begin foo",
        "wrap constructed with a",
        "suspend a"
    };
    ASSERT_EQ(kBeforeFirstSuspend, m_messages);

    triggerAsyncCall();

    static const Messages kAfterFirstSuspend{
        "scope begin test",
        "scope begin foo",
        "wrap constructed with a",
        "suspend a",
        "callback a",
        "result from a",
        "wrap destroyed with a",
        "scope end foo",
        "foo result",
        "wrap constructed with b",
        "suspend b",
    };
    ASSERT_EQ(kAfterFirstSuspend, m_messages);

    delete destructible;

    triggerAsyncCall();

    static const Messages kFull{
        "scope begin test",
        "scope begin foo",
        "wrap constructed with a",
        "suspend a",
        "callback a",
        "result from a",
        "wrap destroyed with a",
        "scope end foo",
        "foo result",
        "wrap constructed with b",
        "suspend b",
        "callback b",
        "wrap destroyed with b",
        "scope end test"
    };
    ASSERT_EQ(kFull, m_messages);
}

TEST_F(TaskTest, WhenAll)
{
    // Result is move-only
    const auto uniqueptrTask =
        [this]() -> Task<std::unique_ptr<std::string>>
        {
            auto self = this;
            co_await self->wrapAsyncCall("a");
            co_return std::make_unique<std::string>("unique_ptr result");
        };

    struct Value
    {
        Value(std::string value): value(std::move(value)) {}
        std::string value;
    };
    static_assert(!std::is_default_constructible_v<Value>);

    // Result is non-default-constructible.
    const auto valueTask =
        [this]() -> Task<Value>
        {
            auto self = this;
            co_await self->wrapAsyncCall("b");
            co_return Value("value result");
        };

    const auto test =
        [&]() -> FireAndForget
        {
            Scope s{"test", this};

            auto [uniqueptrValue, strValue] = co_await whenAll(uniqueptrTask(), valueTask());

            m_messages.push_back(*uniqueptrValue);
            m_messages.push_back(strValue.value);
        };

    test();

    static const Messages kBothSuspended{
        "scope begin test",
        "wrap constructed with a",
        "suspend a",
        "wrap constructed with b",
        "suspend b",
    };
    ASSERT_EQ(kBothSuspended, m_messages);

    triggerAsyncCall();

    static const Messages kGotResultA{
        "scope begin test",
        "wrap constructed with a",
        "suspend a",
        "wrap constructed with b",
        "suspend b",
        "callback a",
        "wrap destroyed with a"
    };
    ASSERT_EQ(kGotResultA, m_messages);

    triggerAsyncCall();

    static const Messages kGotBothResults{
        "scope begin test",
        "wrap constructed with a",
        "suspend a",
        "wrap constructed with b",
        "suspend b",
        "callback a",
        "wrap destroyed with a",
        "callback b",
        "wrap destroyed with b",
        "unique_ptr result",
        "value result",
        "scope end test"
    };
    ASSERT_EQ(kGotBothResults, m_messages);
}

TEST_F(TaskTest, InheritCancelCondition)
{
    auto nested = [this]() -> Task<>
    {
        Scope s{"nested", this};

        co_await cancelIf([&]() { m_messages.push_back("cancel check nested"); return false; });

        m_messages.push_back(co_await wrapAsyncCall("a"));
        m_messages.push_back(co_await wrapAsyncCall("b"));
    };

    const auto test =
        [&]() -> FireAndForget
        {
            Scope s{"test", this};

            co_await cancelIf([&]() { m_messages.push_back("cancel check"); return false; });

            co_await nested();

            m_messages.push_back(co_await wrapAsyncCall("c"));
        };

    test();

    triggerAsyncCall();
    triggerAsyncCall();
    triggerAsyncCall();

    // Note that isCancelRequested() is used by wrapAsyncCall() before resuming because it returns
    // the custom awaiter instead of Task<>.
    static const Messages kFull{
        "scope begin test",
        "cancel check", //< Resume on cancelIf
        "scope begin nested",
        "cancel check nested", //< Resume on cancelIf in nested
        "wrap constructed with a",
        "suspend a",
        "callback a",
        "cancel check", //< Resume after await a, via isCancelRequested()
        "cancel check nested",
        "result from a",
        "wrap destroyed with a",
        "wrap constructed with b",
        "suspend b",
        "callback b",
        "cancel check", //< Resume after await b, via isCancelRequested()
        "cancel check nested",
        "result from b",
        "wrap destroyed with b",
        "scope end nested",
        "cancel check", //< Resume after await nested
        "wrap constructed with c",
        "suspend c",
        "callback c",
        "cancel check", //< Resume after await c, via isCancelRequested()
        "result from c",
        "wrap destroyed with c",
        "scope end test"
    };
    ASSERT_EQ(kFull, m_messages);
}

TEST_F(TaskTest, ResumeInSuspension)
{
    // Resumes directly from await_suspend.
    struct Awaiter
    {
        Awaiter(Messages* messages): m_messages(messages)
        {
            m_messages->emplace_back("awaiter constructed");
        }

        ~Awaiter()
        {
            m_messages->emplace_back("awaiter destroyed");
        }

        bool await_ready() const { return false; }

        void await_suspend(std::coroutine_handle<> h)
        {
            m_messages->emplace_back("awaiter suspend");
            m_result = "result from awaiter";
            auto messages = m_messages; //< `this` can be destroyed after resuming.
            h();
            messages->emplace_back("awaiter after resume finished");
        }

        std::string await_resume() const { return m_result; }

        Messages* m_messages;
        std::string m_result;
    };

    {
        const auto test =
            [&]() -> FireAndForget
            {
                Scope s{"test", this};

                co_await Awaiter(&m_messages);
            };

        test();
    }

    static const Messages kFull{
        "scope begin test",
        "awaiter constructed",
        "awaiter suspend",
        "awaiter destroyed",
        "scope end test",
        // Awaiter is destroyed yet it's await_suspend() continues execution.
        "awaiter after resume finished"
    };
    ASSERT_EQ(kFull, m_messages);
}

} // namespace nx::utils::test
