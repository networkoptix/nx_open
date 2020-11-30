#include "exception.h"

#include <memory>
#include <cstring>

namespace nx::utils {

Exception::~Exception()
{
    delete[] m_whatCache;
}

Exception::Exception(Exception&& other):
    m_whatCache(other.m_whatCache.exchange(nullptr))
{
}

Exception::Exception(const Exception&):
    m_whatCache(nullptr)
{
    // NOTE: It's better to avoid cache copying here and create new one only on-demand.
}

Exception& Exception::operator=(Exception&& other)
{
    m_whatCache = other.m_whatCache.exchange(m_whatCache.load());
    return *this;
}

Exception& Exception::operator=(const Exception&)
{
    // NOTE: It's better to avoid cache copying here and create new one only on-demand.
    clearWhatCache();
    return *this;
}

const char* Exception::what() const noexcept
{
    char* what = m_whatCache.load();
    if (what)
        return what;

    std::string messageStr = message().toStdString();
    std::unique_ptr<char[]> messagePtr(new char[messageStr.size() + 1]);
    std::memcpy(messagePtr.get(), messageStr.data(), messageStr.size() + 1);

    if (m_whatCache.compare_exchange_strong(what, messagePtr.get()))
        return messagePtr.release();
    return what;
}

void Exception::clearWhatCache()
{
    delete[] m_whatCache.exchange(nullptr);
}

ContextedException::ContextedException(const std::string& message):
    m_message(QString::fromStdString(message))
{
}

ContextedException::ContextedException(const std::exception& exception):
    m_message(exception.what())
{
}

ContextedException::ContextedException(QString message):
    m_message(std::move(message))
{
}

void ContextedException::addContext(const std::string& context)
{
    addContext(QString::fromStdString(context));
}

void ContextedException::addContext(const QString& context)
{
    m_message = NX_FMT("%1\ncaused by\n%2", context,  m_message);
    clearWhatCache();
}

QString ContextedException::message() const
{
    return m_message;
}

} // namespace nx::utils
