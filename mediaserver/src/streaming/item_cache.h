////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ITEMCACHE_H
#define ITEMCACHE_H

#include <QCache>

#include "item_cache_data_provider.h"


/*!
    Caches items of \a CachedType.
    Takes ownership of stored items, so any item could be deleted on item insertion.
    \note Not thread-safe
    \note operator< is required from \a KeyType
*/
template<class KeyType, class CachedType>
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

    ItemCache(
        unsigned int maxCost = 100,
        ItemCacheDataProvider<KeyType, CachedType>* const dataProvider = NULL );

    bool insert( const KeyType& key, CachedType* item, unsigned int itemCost = 1 );
    const CachedType* get( const KeyType& key ) const;
    CachedType* get( const KeyType& key );
    bool remove( const KeyType& key );
    unsigned int totalCost() const;
    /*!
        While used, item could not be removed from cache
    */
    CachedType* takeForUse( const KeyType& key );
    void putBackUsedItem( const KeyType& key, CachedType* );
};

#endif  //ITEMCACHE_H
