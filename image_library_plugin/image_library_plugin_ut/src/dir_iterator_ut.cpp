/**********************************************************
* 05 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include <iostream>
#include <fstream>

#include <gtest/gtest.h>

#include "dir_iterator.h"


static const wchar_t* tempDir = L"c:\\temp\\";

TEST( DirIterator, Base )
{
    DirIterator it( tempDir );
    it.setRecursive( true );
    it.setEntryTypeFilter( FsEntryType::etDirectory );
    it.setWildCardMask( L"*m*" );
    for( ; it.next(); )
    {
        std::wcout<<"Found "<<FsEntryType::toString(it.type())<<"\t  "<<it.path()<<std::endl;
    }
}
