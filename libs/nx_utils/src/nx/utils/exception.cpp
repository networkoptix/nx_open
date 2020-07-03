#include "exception.h"

#include <utility>

namespace nx::utils {

Exception::Exception(std::string message):
    m_message(std::move(message))
{
}

Exception::Exception(const QString& message):
    m_message(message.toStdString())
{
}

void Exception::addContext(const std::string& context)
{
    m_message = context + "\ncaused by\n" + m_message;
}

void Exception::addContext(const QString& context)
{
    addContext(context.toStdString());
}

QString Exception::message() const
{
    return QString::fromStdString(m_message);
}

const char* Exception::what() const noexcept
{
    return m_message.data();
}

} // namespace nx::utils

