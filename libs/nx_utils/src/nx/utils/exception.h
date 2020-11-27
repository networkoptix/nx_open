#pragma once

#include <exception>
#include <utility>
#include <string>
#include <tuple>

#include <QString>

#include <nx/utils/log/log_message.h>

namespace nx::utils {

class NX_UTILS_API Exception: public std::exception
{
private:
    struct FutureTranslator
    {
        template <typename Future>
        decltype(auto) operator()(Future&& future) const
        {
            return translate([&]{ return std::forward<Future>(future).get(); });
        };
    };

public:
    explicit Exception(std::string message);
    explicit Exception(const std::exception& exception);
    explicit Exception(const QString& message);

    template <typename... Args>
    explicit Exception(const Args&... args):
        Exception(nx::format(args...).toStdString())
    {
    }

    void addContext(const std::string& context);
    void addContext(const QString& context);

    template <typename... Args>
    void addContext(const Args&... args)
    {
        addContext(nx::format(args...).toStdString());
    }

    virtual QString message() const;

    virtual const char* what() const noexcept override final;

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
                catch (Exception& exception)
                {
                    std::apply([&](const auto&... args) { exception.addContext(args...); }, args);
                    throw;
                }
            };
    }

    template <typename Callable>
    static decltype(auto) translate(Callable&& callable)
    {
        try
        {
            return std::forward<Callable>(callable)();
        }
        catch (const Exception&)
        {
            throw;
        }
        catch (const std::exception& exception)
        {
            throw Exception(exception);
        }
    }

    // MSVC bug prevents this from being a simple lambda
    static constexpr FutureTranslator translateFuture{};

private:
    std::string m_message;
};

} // namespace nx::utils

