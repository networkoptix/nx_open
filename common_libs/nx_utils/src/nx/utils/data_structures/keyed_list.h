#pragma once

#include <type_traits>

#include <QtCore/QList>
#include <QtCore/QHash>

#include <nx/utils/log/assert.h>

namespace nx {
namespace utils {

/**
* Random access container with associated keys for fast lookup by key.
* Insertion is allowed only at front and back and has amortized O(1) complexity.
* Removal is allowed from any place and has O(N) complexity (QList removal).
* Obtaining key or value by index has O(1) complexity.
* Check whether a key exists has O(1) complexity.
* Obtaining index or value by key has O(logN) complexity.
*/
template<typename Key, typename Value>
class KeyedList
{
public:
    KeyedList() = default;
    KeyedList(const KeyedList& other);

    int size() const;
    int count() const { return size(); }
    bool empty() const;

    // O(1) complexity (QHash::contains).
    bool contains(const Key& key) const;

    // Amortized O(1) complexity (QList::push_xxx).
    bool push_back(const Key& key, const Value& value);
    bool push_front(const Key& key, const Value& value);

    // O(N) complexity (QList::removeAt).
    void removeAt(int index);
    bool remove(const Key& key); //< removeAt(index_of(key)) - still O(N).

    void clear();

    // O(logN) complexity (sorted QList binary search).
    int index_of(const Key& key) const;

    // O(1) complexity (QList::operator[]).
    const Key& key(int index) const;

    // O(logN) complexity (sorted QList binary search).
    Value value(const Key& key, const Value& defaultValue = Value()) const;

    // O(1) complexity (QList::operator[]).
    const Value& operator[](int index) const;
    Value& operator[](int index);

    static constexpr int kInvalidIndex = -1;

private:
    struct Item
    {
        Key key;
        Value value;
        qint64 number; //< Sequential item number.
    };

    QList<Item> m_items;
    QHash<Key, qint64> m_keyToNumber;
};

/**
* Random access container of keys with fast lookup by key.
* Insertion is allowed only at front and back and has amortized O(1) complexity.
* Removal is allowed from any place and has O(N) complexity (QList removal).
* Obtaining key by index has O(1) complexity.
* Check whether a key exists has O(1) complexity.
* Obtaining index of key has O(logN) complexity.
*/
template<typename Key>
class KeyList
{
public:
    KeyList() = default;
    KeyList(const KeyList& other);

    template<class KeyContainer>
    KeyList(const KeyContainer& keys)
    {
        qint64 number = 0;
        m_items.reserve(keys.size());

        for (const auto& key: keys)
        {
            m_items.push_back({key, number});
            m_keyToNumber.insert(key, number);
            ++number;
        }
    }

    int size() const;
    int count() const { return size(); }
    bool empty() const;

    // O(1) complexity (QHash::contains).
    bool contains(const Key& key) const;

    // Amortized O(1) complexity (QList::push_xxx).
    bool push_back(const Key& key);
    bool push_front(const Key& key);

    // O(N) complexity (QList::removeAt).
    void removeAt(int index);
    bool remove(const Key& key); //< removeAt(index_of(key)) - still O(N).

    void clear();

    // O(logN) complexity (sorted QList binary search).
    int index_of(const Key& key) const;

    // O(1) complexity (QList::operator[]).
    const Key& key(int index) const;
    const Key& operator[](int index) const { return key(index); }

    static constexpr int kInvalidIndex = -1;

private:
    struct Item
    {
        Key key;
        qint64 number; //< Sequential item number.
    };

    QList<Item> m_items;
    QHash<Key, qint64> m_keyToNumber;
};

// ------------------------------------------------------------------------------------------------
// KeyedList implementation.

template<typename Key, typename Value>
KeyedList<Key, Value>::KeyedList(const KeyedList& other):
    m_items(other.m_items),
    m_keyToNumber(other.m_keyToNumber)
{
}

template<typename Key, typename Value>
int KeyedList<Key, Value>::size() const
{
    return m_items.size();
}

template<typename Key, typename Value>
bool KeyedList<Key, Value>::empty() const
{
    return m_items.empty();
}

template<typename Key, typename Value>
bool KeyedList<Key, Value>::contains(const Key& key) const
{
    return m_keyToNumber.contains(key);
}

template<typename Key, typename Value>
bool KeyedList<Key, Value>::push_back(const Key& key, const Value& value)
{
    if (m_keyToNumber.contains(key))
        return false;

    const auto nextNumber = m_items.empty() ? 0 : m_items.back().number + 1;
    m_items.push_back({key, value, nextNumber});
    m_keyToNumber.insert(key, nextNumber);
    return true;
}

template<typename Key, typename Value>
bool KeyedList<Key, Value>::push_front(const Key& key, const Value& value)
{
    if (m_keyToNumber.contains(key))
        return false;

    const auto nextNumber = m_items.empty() ? -1 : m_items.front().number - 1;
    m_items.push_front({key, value, nextNumber});
    m_keyToNumber.insert(key, nextNumber);
    return true;
}

template<typename Key, typename Value>
void KeyedList<Key, Value>::removeAt(int index)
{
    m_keyToNumber.remove(m_items[index].key);
    m_items.removeAt(index);
}

template<typename Key, typename Value>
bool KeyedList<Key, Value>::remove(const Key& key)
{
    const auto index = index_of(key);
    if (index == kInvalidIndex)
        return false;

    removeAt(index);
    return true;
}

template<typename Key, typename Value>
void KeyedList<Key, Value>::clear()
{
    m_keyToNumber.clear();
    m_items.clear();
}

template<typename Key, typename Value>
int KeyedList<Key, Value>::index_of(const Key& key) const
{
    const auto iter = m_keyToNumber.find(key);
    if (iter == m_keyToNumber.end())
        return kInvalidIndex;

    const auto pos = std::lower_bound(m_items.cbegin(), m_items.cend(), iter.value(),
        [](const Item& left, qint64 rightNumber) { return left.number < rightNumber; });
    NX_EXPECT(pos != m_items.cend() && pos->number == iter.value());
    return std::distance(m_items.cbegin(), pos);
}

template<typename Key, typename Value>
const Key& KeyedList<Key, Value>::key(int index) const
{
    return m_items[index].key;
}

template<typename Key, typename Value>
const Value& KeyedList<Key, Value>::operator[](int index) const
{
    return m_items[index].value;
}

template<typename Key, typename Value>
Value& KeyedList<Key, Value>::operator[](int index)
{
    return m_items[index].value;
}

template<typename Key, typename Value>
Value KeyedList<Key, Value>::value(const Key& key, const Value& defaultValue) const
{
    const auto index = index_of(key);
    return index != kInvalidIndex ? (*this)[index] : defaultValue;
}

// ------------------------------------------------------------------------------------------------
// KeyList implementation.

template<typename Key>
KeyList<Key>::KeyList(const KeyList& other):
    m_items(other.m_items),
    m_keyToNumber(other.m_keyToNumber)
{
}

template<typename Key>
int KeyList<Key>::size() const
{
    return m_items.size();
}

template<typename Key>
bool KeyList<Key>::empty() const
{
    return m_items.empty();
}

template<typename Key>
bool KeyList<Key>::contains(const Key& key) const
{
    return m_keyToNumber.contains(key);
}

template<typename Key>
bool KeyList<Key>::push_back(const Key& key)
{
    if (m_keyToNumber.contains(key))
        return false;

    const auto nextNumber = m_items.empty() ? 0 : m_items.back().number + 1;
    m_items.push_back({key, nextNumber});
    m_keyToNumber.insert(key, nextNumber);
    return true;
}

template<typename Key>
bool KeyList<Key>::push_front(const Key& key)
{
    if (m_keyToNumber.contains(key))
        return false;

    const auto nextNumber = m_items.empty() ? -1 : m_items.front().number - 1;
    m_items.push_front({key, nextNumber});
    m_keyToNumber.insert(key, nextNumber);
    return true;
}

template<typename Key>
void KeyList<Key>::removeAt(int index)
{
    m_keyToNumber.remove(m_items[index].key);
    m_items.removeAt(index);
}

template<typename Key>
bool KeyList<Key>::remove(const Key& key)
{
    const auto index = index_of(key);
    if (index == kInvalidIndex)
        return false;

    removeAt(index);
    return true;
}

template<typename Key>
void KeyList<Key>::clear()
{
    m_keyToNumber.clear();
    m_items.clear();
}

template<typename Key>
int KeyList<Key>::index_of(const Key& key) const
{
    const auto iter = m_keyToNumber.find(key);
    if (iter == m_keyToNumber.end())
        return kInvalidIndex;

    const auto pos = std::lower_bound(m_items.cbegin(), m_items.cend(), iter.value(),
        [](const Item& left, qint64 rightNumber) { return left.number < rightNumber; });
    NX_EXPECT(pos != m_items.cend() && pos->number == iter.value());
    return std::distance(m_items.cbegin(), pos);
}

template<typename Key>
const Key& KeyList<Key>::key(int index) const
{
    return m_items[index].key;
}

} // namespace utils
} // namespace nx
