// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
