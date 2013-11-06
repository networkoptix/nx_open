/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef DIR_ITERATOR_H
#define DIR_ITERATOR_H

#include <string>

#include <stdint.h>


namespace FsEntryType
{
    enum Value
    {
        etRegularFile = 0x01,
        etDirectory = 0x02,
        etOther = 0x04
    };

    const char* toString( Value val );
}



class DirIteratorImpl;

//!Iterates through contents of specified directory
/*!
    Example of usage:\n
    \code {*.cpp}
    for( DirIterator it("/home"); it.next(); )
    {
        //found it.name();
    }
    \endcode

    \note Class is not thread-safe
    \note During search entries '.' and '..' are skipped
*/
class DirIterator
{
public:
    /*!
        Initially, iterator is pointing to some undefined position
        \param dirPath Directory to iterate
    */
    DirIterator( const std::string& dirPath );
    ~DirIterator();

    //!Enable/disable reading child directories. By default recursive mode is off
    void setRecursive( bool _recursive );
    /*!
        By default, no mask is applied
        \param wildcardMask Enable filtering by \a wildcardMask. Empty string - no mask
    */
    void setWildCardMask( const std::string& wildcardMask );
    //!Filter found entries by entry type
    /*!
        By default, all entries are found
        \param entryTypeMask Bit OR- field containing bits of \a EntryType enumeration
    */
    void setEntryTypeFilter( unsigned int entryTypeMask );

    //!Moves iterator to the next entry
    /*!
        \return true, if next found, false - if no entries satisfuying search criteria
    */
    bool next();

    //!Returns path to current entry (path relative to \a dirPath, specified at initialization)
    std::string entryPath() const;
    //!Returns "search dir path" / "entry path"
    std::string entryFullPath() const;
    FsEntryType::Value entryType() const;
    uint64_t entrySize() const;

private:
    DirIteratorImpl* m_impl;
};

#endif  //DIR_ITERATOR_H
