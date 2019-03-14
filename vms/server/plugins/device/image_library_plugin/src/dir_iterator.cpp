/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include "dir_iterator.h"

#ifdef _WIN32
#   include <Windows.h>
#elif __linux__
#   include <sys/types.h>
#   include <dirent.h>
#   include <sys/stat.h>
#   include <errno.h>
#else
#   error "Unsupported platform"
#endif

#include <cstring>

#include <list>

#include "wildcard_match.h"


namespace FsEntryType
{
    const char* toString( Value val )
    {
        switch( val )
        {
            case etRegularFile:
                return "file";
            case etDirectory:
                return "directory";
            default:
                return "unknown";
        }
    }
}

using std::list;
using std::string;
using std::string;

class DirIteratorImpl
{
public:
    string dir;
    bool recursive;
    std::string wildcardMask;
    unsigned int entryTypeMask;
    list<string> dirQueue;
#ifdef _WIN32
    HANDLE hSearch;             // Search handle returned by FindFirstFile
#elif __linux__
    DIR* dp;
#endif

    std::string entryPath;
    std::string entryName;
    FsEntryType::Value entryType;
    //!(uint64_t)-1, if not set
    uint64_t entrySize;

    DirIteratorImpl()
    :
        recursive( false ),
        entryTypeMask( 0 ),
#ifdef _WIN32
        hSearch( INVALID_HANDLE_VALUE ),
#elif __linux__
        dp( NULL ),
#endif
        entryType( FsEntryType::etOther ),
        entrySize( (uint64_t)-1 )
    {
    }

    ~DirIteratorImpl()
    {
#ifdef _WIN32
        if( hSearch != INVALID_HANDLE_VALUE )
        {
            FindClose( hSearch );
            hSearch = INVALID_HANDLE_VALUE;
        }
#elif __linux__
        if( dp != NULL )
        {
            closedir( dp );
            dp = NULL;
        }
#endif
    }


#ifdef _WIN32
    bool next()
    {
        for( ;; )
        {
            if( dirQueue.empty() )
                return false;   //already at end

            const string& currentDir = dirQueue.front();

            WIN32_FIND_DATAA fileData;   // Data structure describes the file found
            memset( &fileData, 0, sizeof(fileData) );

            if( hSearch == INVALID_HANDLE_VALUE )
            {
                hSearch = FindFirstFileA( (dir+"/"+currentDir+"/*").c_str(), &fileData );
                if( hSearch == INVALID_HANDLE_VALUE )
                {
                    dirQueue.clear();
                    return false;
                }
            }
            else if( !FindNextFileA(hSearch, &fileData) )
            {
                const DWORD findErrBak = GetLastError();
                FindClose( hSearch );
                hSearch = INVALID_HANDLE_VALUE;
                if( findErrBak == ERROR_NO_MORE_FILES )
                {
                    //switching to the next directory
                    dirQueue.pop_front();
                    continue;
                }

                //error occured
                dirQueue.clear();
                //restoring last error code, since FindClose overwrites it
                SetLastError( findErrBak );
                return false;
            }

            if( strcmp( fileData.cFileName, "." ) == 0 )
                continue;
            if( strcmp( fileData.cFileName, ".." ) == 0 )
                continue;

            entryName = string(fileData.cFileName);
            entryPath = currentDir.empty()
                ? string(fileData.cFileName)
                : currentDir + "/" + fileData.cFileName;
            if( fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                entryType = FsEntryType::etDirectory;
                //found inline dir
                if( recursive )
                    dirQueue.push_back( entryPath );
            }
            else if( (fileData.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) || 
                     (fileData.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) )
            {
                entryType = FsEntryType::etRegularFile;
            }
            else
            {
                entryType = FsEntryType::etOther;
            }

            entrySize = ((uint64_t)fileData.nFileSizeHigh << 32) | fileData.nFileSizeLow;
            //SYSTEMTIME systemTime;
            //if( !FileTimeToSystemTime( &fileData.ftCreationTime, &systemTime ) )
            //    return false;

      //      struct tm tm1;
            //tm1.tm_year = systemTime.wYear - 1900;
            //tm1.tm_mon = systemTime.wMonth-1;
            //tm1.tm_mday = systemTime.wDay;
            //tm1.tm_hour = systemTime.wHour;
            //tm1.tm_min = systemTime.wMinute;
            //tm1.tm_sec = systemTime.wSecond;
            //dirEntry.creationTimestamp.m_val = mtime::utcTime( tm1 );

            return true;
        }
    }

#elif __linux__

    bool next()
    {
        for( ;; )
        {
            if( dirQueue.empty() )
                return false;   //already at end

            const string& currentDir = dirQueue.front();

            if( dp == NULL )
            {
                dp = opendir( (dir+"/"+currentDir).c_str() );
                if( dp == NULL )
                    return false;
            }

            struct dirent entry;
            struct dirent* result;
            if( readdir_r( dp, &entry, &result ) != 0 )
            {
                //error occured
                const int findErrBak = errno;
                closedir( dp );
                dp = NULL;
                dirQueue.clear();
                //restoring last error code, since closedir overwrites it
                errno = findErrBak;
                return false;
            }

            if( result == NULL )    //no more entries
            {
                //next directory
                closedir( dp );
                dp = NULL;
                dirQueue.pop_front();
                continue;
            }

            if( !strcmp( result->d_name, "." ) )
                continue;
            if( !strcmp( result->d_name, ".." ) )
                continue;

            entryName = string(result->d_name);
                entryPath = currentDir.empty() ? string(result->d_name) : currentDir+"/"+result->d_name;

#ifdef _BSD_SOURCE
            entrySize = (uint64_t)-1;
            switch( result->d_type )
            {
                case DT_DIR:
                    entryType = FsEntryType::etDirectory;
                    if( recursive )
                        dirQueue.push_back( entryPath );
                    return true;

                case DT_REG:
                    entryType = FsEntryType::etRegularFile;
                    return true;

                case DT_UNKNOWN:
                    break;  //performing stat

                default:
                    entryType = FsEntryType::etOther;
                    return true;
            }
#endif
            struct stat st;    //linux uses most recent API (stat64 since 2.4.x)
            const string fullName = dir+"/"+currentDir+"/"+string(result->d_name);
            if( stat( fullName.c_str(), &st ) )
                continue;

            if( st.st_mode & S_IFDIR )
            {
                entryType = FsEntryType::etDirectory;
                if( recursive )
                    dirQueue.push_back( entryPath );
            }
            else if( st.st_mode & S_IFREG )
            {
                entryType = FsEntryType::etRegularFile;
            }
            else
            {
                entryType = FsEntryType::etOther;
            }
            entrySize = st.st_size;
            //creationTimestamp.m_val = st.st_ctime;

            return true;
        }
    }
#endif
};


DirIterator::DirIterator( const std::string& dirPath )
:
    m_impl( new DirIteratorImpl() )
{
    m_impl->dir = dirPath;
    m_impl->dirQueue.push_back( string() );
}

DirIterator::~DirIterator()
{
    delete m_impl;
}

void DirIterator::setRecursive( bool _recursive )
{
    m_impl->recursive = _recursive;
}

void DirIterator::setWildCardMask( const std::string& wildcardMask )
{
    m_impl->wildcardMask = wildcardMask;
}

void DirIterator::setEntryTypeFilter( unsigned int entryTypeMask )
{
    m_impl->entryTypeMask = entryTypeMask;
}

bool DirIterator::next()
{
    while( m_impl->next() )
    {
        //applying filter
        if( !(m_impl->entryTypeMask == 0 || (m_impl->entryTypeMask & m_impl->entryType) > 0) )
            continue;
        if( !(m_impl->wildcardMask.empty() || wildcardMatch(m_impl->wildcardMask.c_str(), m_impl->entryName.c_str())) )
            continue;

        return true;
    }

    return false;
}

std::string DirIterator::entryPath() const
{
    return m_impl->entryPath;
}

std::string DirIterator::entryFullPath() const
{
    return m_impl->dir + "/" + m_impl->entryPath;
}

FsEntryType::Value DirIterator::entryType() const
{
    return m_impl->entryType;
}

uint64_t DirIterator::entrySize() const
{
#ifdef __linux__
    if( m_impl->entrySize == (uint64_t)-1 )
    {
        //performing stat here, because unneeded stat64 call can greately slow down directory traversal
        struct stat64 st;
        memset( &st, 0, sizeof(st) );
        if( stat64( (m_impl->dir + "/" + m_impl->entryPath).c_str(), &st ) )
            return 0;
        m_impl->entrySize = st.st_size;
    }
#endif
    return m_impl->entrySize;
}
