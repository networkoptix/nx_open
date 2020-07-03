#pragma once

#include <exception>
#include <string>

#include <QString>

#include <nx/utils/log/log_message.h>

namespace nx::utils {

class NX_UTILS_API Exception: public std::exception
{
public:
    explicit Exception(std::string message);

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

private:
    std::string m_message;
};

} // namespace nx::utils

