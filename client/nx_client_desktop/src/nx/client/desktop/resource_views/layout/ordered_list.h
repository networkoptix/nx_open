#pragma once

#include <QtCore/QList>

#include <utils/math/math.h>

template<typename Key, typename Data>
class OrderedList
{
    struct Item
    {
        Key key;
        Data data;
        bool operator<(const Item& right) const;
    };

    using Storage = QList<Item>;

public:
    using iterator = typename Storage::iterator;
    using const_iterator = typename Storage::const_iterator;

    void insert(const Key& key, const Data& data);
    void removeAt(int index);

    const Data& value(const Key& index) const;
    const Data& at(int index) const;

    const Key& keyAt(int index) const;

    int indexOfKey(const Key& key) const;

    const_iterator find(const Key& key) const;
    iterator find(const Key& key);

    const_iterator begin() const;
    iterator begin();

    const_iterator end() const;
    iterator end();

    bool contains(const Key& key) const;

    int size() const;

private:
    QList<Item> m_data;
};

template<typename Key, typename Data>
void OrderedList<Key, Data>::insert(const Key& key, const Data& data)
{
    const Item item({key, data});
    const auto it = std::lower_bound(m_data.begin(), m_data.end(), item);
    m_data.insert(it, item);
}

template<typename Key, typename Data>
void OrderedList<Key, Data>::removeAt(int index)
{
    m_data.removeAt(index);
}

template<typename Key, typename Data>
const Data& OrderedList<Key, Data>::value(const Key& key) const
{
    const auto it = find(key);
    if (it != end())
        return it->data;

    NX_EXPECT(false, "Can't find key");
    return Data();
}

template<typename Key, typename Data>
const Data& OrderedList<Key, Data>::at(int index) const
{
    return m_data.at(index).data;
}

template<typename Key, typename Data>
const Key& OrderedList<Key, Data>::keyAt(int index) const
{
    return m_data.at(index).key;
}

template<typename Key, typename Data>
int OrderedList<Key, Data>::indexOfKey(const Key& key) const
{
    const auto it = find(key);
    return it == end() ? -1 : it - begin();
}

template<typename Key, typename Data>
typename OrderedList<Key, Data>::const_iterator OrderedList<Key, Data>::find(const Key& key) const
{
    const Item item({key, Data()});
    const auto it = std::lower_bound(m_data.begin(), m_data.end(), item);
    return it != m_data.end() && it->key == key ? it : m_data.end();
}

template<typename Key, typename Data>
typename OrderedList<Key, Data>::iterator OrderedList<Key, Data>::find(const Key& key)
{
    const Item item({key, Data()});
    const auto it = std::lower_bound(m_data.begin(), m_data.end(), item);
    return it != m_data.end() && it->key == key ? it : m_data.end();
}

template<typename Key, typename Data>
typename OrderedList<Key, Data>::const_iterator OrderedList<Key, Data>::begin() const
{
    return m_data.begin();
}

template<typename Key, typename Data>
typename OrderedList<Key, Data>::iterator OrderedList<Key, Data>::begin()
{
    return m_data.begin();
}

template<typename Key, typename Data>
typename OrderedList<Key, Data>::const_iterator OrderedList<Key, Data>::end() const
{
    return m_data.end();
}

template<typename Key, typename Data>
typename OrderedList<Key, Data>::iterator OrderedList<Key, Data>::end()
{
    return m_data.end();
}

template<typename Key, typename Data>
bool OrderedList<Key, Data>::contains(const Key& key) const
{
    return find(key) != end();
}

template<typename Key, typename Data>
int OrderedList<Key, Data>::size() const
{
    return m_data.size();
}

template<typename Key, typename Data>
bool OrderedList<Key, Data>::Item::operator<(const Item& right) const
{
    return key < right.key;
}

