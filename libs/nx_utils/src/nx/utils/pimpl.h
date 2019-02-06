#pragma once

#include <memory>

namespace nx::utils {

// Const-aware pointer to implementation.
template<typename T>
class PImpl final
{
    using value_type = std::remove_reference_t<T>;
    using const_value_type = const value_type;

public:
    explicit PImpl(value_type* ptr): m_ptr(ptr) {}
    ~PImpl() = default;

    PImpl() = delete;
    PImpl(const PImpl&) = delete;
    PImpl(PImpl&&) = delete;
    PImpl& operator=(const PImpl&) = delete;
    PImpl& operator=(PImpl&&) = delete;

    const_value_type* get() const { return m_ptr.get(); }
    value_type* get() { return m_ptr.get(); }

    const_value_type* data() const { return get(); }
    value_type* data() { return get(); }

    const_value_type* operator->() const { return get(); }
    value_type* operator->() { return get(); }

    const_value_type& operator*() const { return *get(); }
    value_type& operator*() { return *get(); }

private:
    const std::unique_ptr<value_type> m_ptr;
};

} // namespace nx::utils
