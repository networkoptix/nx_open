/**********************************************************
* 11 feb 2014
* a.kolesnikov
***********************************************************/

#ifndef STORED_FILE_MANAGER_IMPL_H
#define STORED_FILE_MANAGER_IMPL_H

#include <QByteArray>
#include <QString>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/ec2_stored_file_data.h>


namespace ec2
{
    //!Does actual work with stored files
    class StoredFileManagerImpl
    {
    public:
        ErrorCode saveFile( const ApiStoredFileData& fileData );
        ErrorCode removeFile( const StoredFilePath& path );
        ErrorCode listDirectory( const StoredFilePath& path, ApiStoredDirContents& data );
        ErrorCode readFile( const StoredFilePath& path, ApiStoredFileData& data );
    };
}

#endif  //STORED_FILE_MANAGER_IMPL_H
