#pragma once

#include <type_traits>

#include <nx/sdk/result.h>
#include <nx/sdk/ptr.h>

namespace nx::vms::server::sdk_support {

namespace detail {

template<typename T>
using RemovedPointer = std::remove_pointer_t<std::remove_reference_t<T>>;

template<typename T>
using CleanType = std::remove_cv_t<RemovedPointer<T>>;

template<typename T>
constexpr const bool IsPointerToRefCountable =
    std::is_pointer<std::remove_cv_t<std::remove_reference_t<T>>>::value
    && std::is_base_of<nx::sdk::IRefCountable, CleanType<T>>::value;

template<typename T>
using EnableForPointerToRefCountable = std::enable_if_t<IsPointerToRefCountable<T>>;

template<typename T>
using DisableForPointerToRefCountable = std::enable_if_t<!IsPointerToRefCountable<T>>;

template<typename Value>
class ResultHolderBase
{
protected:
    ResultHolderBase(nx::sdk::ErrorCode errorCode, const nx::sdk::IString* errorMessage):
        m_errorCode(errorCode),
        m_errorMessage(nx::sdk::toPtr(errorMessage))
    {
    }

public:
    nx::sdk::ErrorCode errorCode() const { return m_errorCode; }

    QString errorMessage() const
    {
        if (m_errorMessage)
            return m_errorMessage->str();

        return QString();
    }

    bool isOk() const
    {
        return errorCode() == nx::sdk::ErrorCode::noError && errorMessage().isEmpty();
    }

private:
    const nx::sdk::ErrorCode m_errorCode;
    const nx::sdk::Ptr<const nx::sdk::IString> m_errorMessage;
};

} // namespace detail

template<typename Value, typename = void>
class ResultHolder;

template<typename Value>
class ResultHolder<Value, typename detail::EnableForPointerToRefCountable<Value>>:
    public detail::ResultHolderBase<Value>
{
public:
    ResultHolder(nx::sdk::Result<Value>&& result):
        detail::ResultHolderBase<Value>(result.error.errorCode, result.error.errorMessage),
        m_value(nx::sdk::toPtr(result.value))
    {
    }

    const nx::sdk::Ptr<typename detail::RemovedPointer<Value>> value() const { return m_value; }

    void isRefCountable() {};

private:
    const nx::sdk::Ptr<typename detail::RemovedPointer<Value>> m_value;
};

template<typename Value>
class ResultHolder<Value, typename detail::DisableForPointerToRefCountable<Value>>:
    public detail::ResultHolderBase<Value>
{
public:
    ResultHolder(nx::sdk::Result<Value>&& result):
        detail::ResultHolderBase<Value>(result.error.errorCode, result.error.errorMessage),
        m_value(std::move(result.value))
    {
    }

    nx::sdk::Ptr<Value> value() const { return m_value; }

    void isNonRefCountable() {};

private:
    const Value m_value;
};

template<>
class ResultHolder<void>: public detail::ResultHolderBase<void>
{
public:
    ResultHolder(nx::sdk::Result<void>&& result):
        detail::ResultHolderBase<void>(result.error.errorCode, result.error.errorMessage)
    {
    }

    void isVoid() {};
};

} // namespace nx::vms::server::sdk_support
