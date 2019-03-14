#pragma once

#include <nx/network/aio/timer.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace network {

struct TemporaryKeyOptions
{
    std::chrono::milliseconds lifeTime{std::chrono::hours(1)};
    bool prolongLifeOnUse = false;
};

template<typename Value>
class TemporaryKeyKeeper
{
public:
    using Key = nx::String;
    using Binding = nx::String;

    TemporaryKeyKeeper(TemporaryKeyOptions options = TemporaryKeyOptions());
    virtual ~TemporaryKeyKeeper();

    TemporaryKeyOptions getOptions() const;
    void setOptions(TemporaryKeyOptions options);

    nx::String make(Value value, Binding binding = {});
    bool addNew(const Key& key, Value value, Binding binding = {});
    void addOrUpdate(const Key& key, Value value, Binding binding = {});

    std::optional<Value> get(const Key& key, const Binding& binding = {}) const;
    void remove(const Key& key);

    template<typename Predicate /* bool(Key, Value, Binding) */>
    size_t removeIf(Predicate predicate);
    size_t size() const;

private:
    struct ValueContext
    {
        Value value;
        Binding binding;
        mutable std::chrono::steady_clock::time_point lastUseTime;
        std::unique_ptr<aio::Timer> expirationTimer;
        bool isRemoved = false;
    };

    using ValueMap = std::map<Key, ValueContext>;
    using ValueIterator = typename ValueMap::iterator;
    using ValueConstIterator = typename ValueMap::const_iterator;

    bool add(const Key& key, Value value, Binding binding, bool updateOnClash);
    void updateTimer(ValueIterator contextIt, std::chrono::steady_clock::time_point now);
    bool isAlive(const ValueContext& context, std::chrono::steady_clock::time_point now) const;

    static QString contextString(const Key& key, const ValueContext& context);
    static QString contextString(ValueConstIterator contextIt);

private:
    mutable QnMutex m_mutex;
    TemporaryKeyOptions m_options;
    aio::BasicPollable m_threadBinding;
    ValueMap m_values;
};

//-------------------------------------------------------------------------------------------------

template<typename Value>
TemporaryKeyKeeper<Value>::TemporaryKeyKeeper(TemporaryKeyOptions options):
    m_options(std::move(options))
{
    NX_CRITICAL(m_options.lifeTime > std::chrono::milliseconds::zero());
}

template<typename Value>
TemporaryKeyKeeper<Value>::~TemporaryKeyKeeper()
{
    m_threadBinding.executeInAioThreadSync(
        [this]()
        {
            QnMutexLocker locker(&m_mutex);
            m_values.clear();
        });
}

template<typename Value>
TemporaryKeyOptions TemporaryKeyKeeper<Value>::getOptions() const
{
    QnMutexLocker locker(&m_mutex);
    return m_options;
}

template<typename Value>
void TemporaryKeyKeeper<Value>::setOptions(TemporaryKeyOptions options)
{
    QnMutexLocker locker(&m_mutex);
    m_options = std::move(options);
    NX_CRITICAL(m_options.lifeTime > std::chrono::milliseconds::zero());
    // No need to update timers, they do so automaticly.
}

template<typename Value>
typename TemporaryKeyKeeper<Value>::Key TemporaryKeyKeeper<Value>::make(Value value, Binding binding)
{
    const auto key = QnUuid::createUuid().toSimpleByteArray();
    const auto result = addNew(key, std::move(value), std::move(binding));
    NX_CRITICAL(result, lm("Random UUID %1 clashed with existing").arg(key));
    return key;
}

template<typename Value>
bool TemporaryKeyKeeper<Value>::addNew(const Key& key, Value value, Binding binding)
{
    return add(key, std::move(value), std::move(binding), /*updateOnClash*/ false);
}

template<typename Value>
void TemporaryKeyKeeper<Value>::addOrUpdate(const Key& key, Value value, Binding binding)
{
    add(key, std::move(value), std::move(binding), /*updateOnClash*/ true);
}

template<typename Value>
std::optional<Value> TemporaryKeyKeeper<Value>::get(const Key& key, const Binding& binding) const
{
    QnMutexLocker locker(&m_mutex);
    const auto it = m_values.find(key);
    if (it == m_values.end())
        return std::nullopt;

    const auto& context = it->second;
    const auto now = std::chrono::steady_clock::now();
    if (!isAlive(context, now))
        return std::nullopt;

    if (!binding.isEmpty() && context.binding != binding)
    {
        // May be a sign of hucker attack attempt.
        NX_WARNING(this, lm("Binding %1 does not match for %2").args(binding, contextString(it)));
        return std::nullopt;
    }

    if (m_options.prolongLifeOnUse)
        context.lastUseTime = std::chrono::steady_clock::now();

    NX_VERBOSE(this, lm("Used %1%2").args(
        contextString(it),
        m_options.prolongLifeOnUse ? ", prolonged" : ""));

    return context.value;
};

template<typename Value>
void TemporaryKeyKeeper<Value>::remove(const Key& key)
{
    QnMutexLocker locker(&m_mutex);
    const auto it = m_values.find(key);
    if (it == m_values.end())
        return;

    NX_DEBUG(this, lm("Remove %1").args(contextString(it)));
    it->second.isRemoved = true;
}

template<typename Value>
template<typename Predicate>
size_t TemporaryKeyKeeper<Value>::removeIf(Predicate predicate)
{
    const auto now = std::chrono::steady_clock::now();

    QnMutexLocker locker(&m_mutex);
    size_t removedCount = 0;
    for (auto& [key, context]: m_values)
    {
        if (isAlive(context, now) && predicate(key, context.value, context.binding))
        {
            NX_DEBUG(this, lm("Remove %1 by predicate").args(contextString(key, context)));
            context.isRemoved = true;
            ++removedCount;
        }
    }

    return removedCount;
}

template<typename Value>
size_t TemporaryKeyKeeper<Value>::size() const
{
    const auto now = std::chrono::steady_clock::now();

    QnMutexLocker locker(&m_mutex);
    size_t aliveCount = 0;
    for (const auto& it: m_values)
    {
        if (isAlive(it.second, now))
            ++aliveCount;
    }

    return aliveCount;
}

template<typename Value>
bool TemporaryKeyKeeper<Value>::add(const Key& key, Value value, Binding binding, bool updateOnClash)
{
    const auto now = std::chrono::steady_clock::now();
    if (key.trimmed().isEmpty())
    {
        NX_ASSERT(false, "Attempt to add empty key");
        return false;
    }

    QnMutexLocker locker(&m_mutex);
    const auto [iterator, isInserted] = m_values.emplace(key, ValueContext{
        {}, {}, now, std::make_unique<aio::Timer>(m_threadBinding.getAioThread()), /*isRemoved*/ false});

    auto& context = iterator->second;
    if (isInserted)
    {
        context.value = std::move(value);
        context.binding = std::move(binding);
        NX_DEBUG(this, lm("New %1").arg(contextString(iterator)));
        updateTimer(iterator, now);
        return true;
    }

    if (!isAlive(context, now))
    {
        context.value = std::move(value);
        context.binding = std::move(binding);
        NX_DEBUG(this, lm("Update previously %1 %2").args(
            context.isRemoved ? "removed" : "expired", contextString(iterator)));

        context.isRemoved = false;
        context.lastUseTime = now;
        return true;
    }

    if (updateOnClash)
    {
        context.value = std::move(value);
        context.binding = std::move(binding);
        NX_DEBUG(this, lm("Update clashed %1").arg(contextString(iterator)));
        context.lastUseTime = now;
    }

    return false;
}

template<typename Value>
void TemporaryKeyKeeper<Value>::updateTimer(
    ValueIterator contextIt, std::chrono::steady_clock::time_point now)
{
    const auto timeLeft = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - contextIt->second.lastUseTime + m_options.lifeTime);

    contextIt->second.expirationTimer->start(
        timeLeft,
        [this, contextIt]()
        {
            const auto now = std::chrono::steady_clock::now();

            QnMutexLocker locker(&m_mutex);
            if (isAlive(contextIt->second, now))
                return updateTimer(contextIt, now); // Lifetime was prolonged.

            NX_DEBUG(this, lm("Remove expired %1").arg(contextString(contextIt)));
            m_values.erase(contextIt);
        });
}

template<typename Value>
bool TemporaryKeyKeeper<Value>::isAlive(
    const ValueContext& context, std::chrono::steady_clock::time_point now) const
{
    return !context.isRemoved && context.lastUseTime + m_options.lifeTime > now;
}

template<typename Value>
QString TemporaryKeyKeeper<Value>::contextString(const Key& key, const ValueContext& context)
{
    return lm("key %1, value %2%3%4").args(
        key, context.value,
        context.binding.isEmpty() ? "" : ", binding ", context.binding);
}

template<typename Value>
QString TemporaryKeyKeeper<Value>::contextString(ValueConstIterator contextIt)
{
    return contextString(contextIt->first, contextIt->second);
}

} // namespace network
} // namespace nx
