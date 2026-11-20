// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <exception>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include <QtCore/QString>

#include <nx/utils/log/format.h>

namespace nx::utils {

class NX_UTILS_API Exception: public std::exception
{
public:
    Exception() = default;
    virtual ~Exception();
    Exception(Exception&& other) noexcept;
    Exception(const Exception& other);
    Exception& operator=(Exception&& other) noexcept;
    Exception& operator=(const Exception& other);

    virtual QString message() const = 0;
    virtual const char* what() const noexcept override final;

protected:
    void clearWhatCache();  //< Not thread-safe.

private:
    mutable std::atomic<char*> m_whatCache = nullptr;
};

// TODO: Add macros for multiple original exceptions.

namespace detail {

#if defined(__clang__) && defined(_MSC_VER) //< Clang-cl.

/**
 * Glues an exception to std::nested_exception, the same way the Standard Library does inside
 * std::throw_with_nested(). Clang-cl emits broken exception handling data for the Standard Library
 * glue class, which makes copying such an exception - something std::current_exception() does -
 * recurse infinitely until the stack is exhausted. Unlike the Standard Library one, this class has
 * no constructor template, which is what the broken exception handling data refers to.
 * See https://github.com/llvm/llvm-project/issues/191462.
 */
template<typename Error>
struct WithNested: Error, std::nested_exception
{
    explicit WithNested(Error error): Error(std::move(error)) {}
};

#endif // Clang-cl.

/**
 * Same as std::throw_with_nested(), which cannot be used directly because of the Clang-cl issue
 * described above.
 */
template<typename Error>
[[noreturn]] void throwWithNested(Error error)
{
#if defined(__clang__) && defined(_MSC_VER)
    if constexpr (std::is_class_v<Error> && !std::is_base_of_v<std::nested_exception, Error>
        && !std::is_final_v<Error>)
    {
        throw WithNested<Error>(std::move(error));
    }
    else
    {
        throw std::move(error);
    }
#else
    std::throw_with_nested(std::move(error));
#endif
}

} // namespace detail

/**
 * Evaluates EXPRESSION. If it throws ORIGINAL_EXCEPTION, throws NEW_EXCEPTION(MESSAGE) with the
 * original exception nested, as std::throw_with_nested() does (see detail::throwWithNested()).
 *
 * unwrapNestedErrors() should be used to get output string for wrapped exceptions.
 */
#define NX_WRAP_EXCEPTION(EXPRESSION, ORIGINAL_EXCEPTION, NEW_EXCEPTION, MESSAGE) \
    [&]() \
    { \
        try \
        { \
            return EXPRESSION; \
        } \
        catch (const ORIGINAL_EXCEPTION&) \
        { \
            ::nx::utils::detail::throwWithNested(NEW_EXCEPTION(MESSAGE)); \
        } \
    }()

class NX_UTILS_API ContextedException: public Exception
{
private:
    struct FutureTranslator
    {
        template <typename Future>
        decltype(auto) operator()(Future&& future) const
        {
            return translate([&] { return std::forward<Future>(future).get(); });
        };
    };

public:
    explicit ContextedException(const std::string& message);
    explicit ContextedException(const std::exception& exception);
    explicit ContextedException(QString message);

    template <typename... Args>
    explicit ContextedException(const Args&... args):
        ContextedException(nx::format(args...).toQString())
    {
    }

    void addContext(const std::string& context);
    void addContext(const QString& context);

    template <typename... Args>
    void addContext(const Args&... args)
    {
        addContext(nx::format(args...).toQString());
    }

    virtual QString message() const override;

#if 0 //< MSVS c++-20 compiler internal error workaround. Revert when fixed.

    template <typename... Args>
    static auto addFutureContext(Args&&... args)
    {
        return
            [args = std::make_tuple(std::forward<Args>(args)...)](auto&& future)
            {
                try
                {
                    return std::forward<decltype(future)>(future).get();
                }
                catch (ContextedException& exception)
                {
                    std::apply([&](const auto&... args) { exception.addContext(args...); }, args);
                    throw;
                }
            };
    }

#else

    template <typename T1, typename T2, typename T3>
    static auto addFutureContext(T1&& arg1, T2&& arg2, T3&& arg3)
    {
        return
            [=](auto&& future)
            {
                try
                {
                    return std::forward<decltype(future)>(future).get();
                }
                catch (ContextedException& exception)
                {
                    exception.addContext(arg1, arg2, arg3);
                    throw;
                }
            };
    }

    template <typename T1, typename T2>
    static auto addFutureContext(T1&& arg1, T2&& arg2)
    {
        return
            [=](auto&& future)
            {
                try
                {
                    return std::forward<decltype(future)>(future).get();
                }
                catch (ContextedException& exception)
                {
                    exception.addContext(arg1, arg2);
                    throw;
                }
            };
    }

    template <typename T1>
    static auto addFutureContext(T1&& arg1)
    {
        return
            [=](auto&& future)
            {
                try
                {
                    return std::forward<decltype(future)>(future).get();
                }
                catch (ContextedException& exception)
                {
                    exception.addContext(arg1);
                    throw;
                }
            };
    }

#endif //< MSVS c++-20 compiler internal error workaround

    template <typename Callable>
    static decltype(auto) translate(Callable&& callable)
    {
        try
        {
            return std::forward<Callable>(callable)();
        }
        catch (const ContextedException&)
        {
            throw;
        }
        catch (const std::exception& exception)
        {
            throw ContextedException(exception);
        }
    }

    // MSVC bug prevents this from being a simple lambda
    static constexpr FutureTranslator translateFuture{};

private:
    QString m_message;
};

} // namespace nx::utils
