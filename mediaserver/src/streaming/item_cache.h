////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ITEMCACHE_H
#define ITEMCACHE_H

#include <map>

#include <QCache>
#include <QMutex>
#include <QMutexLocker>


template<class KeyType, class CachedType>
class DummyItemProvider
{
public:
    /*!
        \param itemCost If item is created, its cost returned with this argument. MUST NOT be NULL
    */
    CachedType* get( const KeyType& key, int* const itemCost ) throw() { return NULL; }
};

/*!
    Caches items of \a CachedType.
    Takes ownership of stored items, so any item could be deleted on item insertion.
    \param KeyType MUST have operator < defined
    \note All methods are thread-safe
    \note operator== and qHash(KeyType) is required for \a KeyType
    \todo make cache 2-level: introduce persistent storage
*/
template<class KeyType, class CachedType, class ItemProviderType = DummyItemProvider<KeyType, CachedType> >
class ItemCache
{
public:
    typedef KeyType key_type;
    typedef CachedType mapped_type;
    typedef size_t size_type;

    class ScopedItemUsage
    {
    public:
        ScopedItemUsage( ItemCache* const cache, const KeyType& key );
        ~ScopedItemUsage();

        const KeyType& key() const;
        CachedType* item();
    };

    /*!
        \param dataProvider Not owned by cache
    */
    ItemCache(
        int maxCost = 100,
        ItemProviderType* const itemProvider = NULL )
    :
        m_cache( maxCost ),
        m_itemProvider( itemProvider ),
        m_usedItemsTotalCost( 0 ),
        m_maxCost( maxCost )
    {
    }

    /*!
        \param item In case of successful insertion \a item ownership is taken by cache
        \return true, if \a item has been inserted correctly. Item NOT inserted if \a cost is larger than \a maxCost
        \note If item with \a key already exists and it is not used, it will be replaced (previous object is removed)
    */
    bool insert( const KeyType& key, CachedType* item, unsigned int cost = 1 ) throw()
    {
        QMutexLocker lk( &m_mutex );
        return insertNonSafe( key, item, cost );
    }

    /*!
        If item with \a key is not found in cache, its requested from item provider and, if found, added to cache. 
        So, in this method some item(s) could be removed
        \return NULL, if item with \a key not found. Returned item is still owned by cache and can be removed on next insertion to cache
    */
    CachedType* get( const KeyType& key ) throw()
    {
        QMutexLocker lk( &m_mutex );

        CacheItemData* itemData = m_cache.object( key );
        if( itemData )
            return itemData->item;

        if( !m_itemProvider )
            return NULL;

        //requesting item from item provider
        int itemCost = 1;
        CachedType* item = m_itemProvider->get( key, &itemCost );
        if( !item )
            return NULL;

        if( !insertNonSafe( key, item, itemCost ) )
        {
            delete item;
            return NULL;
        }

        return item;
    }

    /*!
        \return true, if item removed or not exist. false, if item could not be removed, since it's in use
    */
    bool remove( const KeyType& key ) throw()
    {
        QMutexLocker lk( &m_mutex );

        if( m_usedItems.find( key ) != m_usedItems.end() )
            return false;   //item exists and is used, cannot remove it

        m_cache.remove( key );
        return true;
    }

    unsigned int totalCost() const throw()
    {
        QMutexLocker lk( &m_mutex );

        return m_cache.totalCost() + m_usedItemsTotalCost;
    }

    /*!
        While used, item could not be removed from cache
    */
    CachedType* takeForUse( const KeyType& key ) throw()
    {
        QMutexLocker lk( &m_mutex );

        typename std::map<KeyType, CacheItemData*>::iterator usedItemIter = m_usedItems.find( key );
        if( usedItemIter != m_usedItems.end() )
        {
            ++usedItemIter->second->usageCount;
            return usedItemIter->second->item;
        }

        CacheItemData* itemData = m_cache.take( key );
        if( itemData )
        {
            //reserving space for tajen item in cache
            const int cacheSizeBak = m_cache.size();
            m_cache.setMaxCost( m_cache.maxCost()-itemData->cost );
            Q_ASSERT_X(
                cacheSizeBak == m_cache.size(),
                "ItemCache::takeForUse",
                QString("cacheSizeBak = %1, m_cache.size() = %2, m_cache.maxCost() = %3").arg(cacheSizeBak).arg(m_cache.size()).arg(m_cache.maxCost()).toLatin1().data() );
        }
        else
        {
            if( !m_itemProvider )
                return NULL;

            //requesting item from item provider
            int itemCost = 1;
            CachedType* item = m_itemProvider->get( key, &itemCost );
            if( !item )
                return NULL;
            itemData = new CacheItemData( item, itemCost );

            if( m_cache.maxCost() > itemData->cost )
                m_cache.setMaxCost( m_cache.maxCost() - itemData->cost );
        }

        m_usedItemsTotalCost += itemData->cost;

        ++itemData->usageCount;
        m_usedItems.insert( std::make_pair( key, itemData ) );
        return itemData->item;
    }

    /*!
        \return false, if \a item was not taken for usage. true, otherwise
    */
    bool putBackUsedItem( const KeyType& key, CachedType* /*item*/ ) throw()
    {
        QMutexLocker lk( &m_mutex );

        typename std::map<KeyType, CacheItemData*>::iterator usedItemIter = m_usedItems.find( key );
        if( usedItemIter == m_usedItems.end() )
            return false;

        Q_ASSERT( usedItemIter->second->usageCount > 0 );
        --usedItemIter->second->usageCount;
        if( usedItemIter->second->usageCount > 0 )
            return true;

        //returning item back to the cache
        m_usedItemsTotalCost -= usedItemIter->second->cost;
        if( m_cache.maxCost() + usedItemIter->second->cost <= m_maxCost )
        {
            m_cache.setMaxCost( m_cache.maxCost() + usedItemIter->second->cost );
        }
        else if( usedItemIter->second->cost < m_maxCost )
        {
            //freeing space for an item
            m_cache.setMaxCost( m_maxCost - usedItemIter->second->cost );
            m_cache.setMaxCost( m_maxCost );
        }
        if( !m_cache.insert( key, usedItemIter->second, usedItemIter->second->cost ) )
        {
            //this can happen in case when item cost is greater than allowed cache cost
        }
        m_usedItems.erase( usedItemIter );
        return true;
    }

    ItemProviderType* itemProvider() const
    {
        QMutexLocker lk( &m_mutex );
        return m_itemProvider;
    }

private:
    class CacheItemData
    {
    public:
        CachedType* item;
        int cost;
        int usageCount;

        CacheItemData()
        :
            item( NULL ),
            cost( 0 ),
            usageCount( 0 )
        {
        }

        CacheItemData(
            CachedType* _item,
            int _cost )
        :
            item( _item ),
            cost( _cost ),
            usageCount( 0 )
        {
        }

        ~CacheItemData()
        {
            delete item;
            item = NULL;
        }
    };

    QCache<KeyType, CacheItemData> m_cache;
    std::map<KeyType, CacheItemData*> m_usedItems;
    ItemProviderType* const m_itemProvider;
    qint64 m_usedItemsTotalCost;
    mutable QMutex m_mutex;
    const qint64 m_maxCost;

    bool insertNonSafe( const KeyType& key, CachedType* item, unsigned int cost = 1 ) throw()
    {
        if( cost > m_cache.maxCost() )
            return false;

        if( m_usedItems.find( key ) != m_usedItems.end() )
            return false;   //item exists and is used, cannot replace it
        CacheItemData* itemData = new CacheItemData( item, cost );
        if( !m_cache.insert( key, itemData, cost ) )
        {
            itemData.item = NULL;
            delete itemData;
            return false;
        }
        return true;
    }
};

#endif  //ITEMCACHE_H
