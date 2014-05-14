/**********************************************************
* 29 apr 2013
* a.kolesnikov
***********************************************************/

#include "ext_iodevice_storage.h"

#include <QtCore/QMutexLocker>


QnExtIODeviceStorageResource::QnExtIODeviceStorageResource()
{
}

QnExtIODeviceStorageResource::~QnExtIODeviceStorageResource()
{
    for( std::map<QString, QIODevice*>::iterator
        it = m_urlToDevice.begin();
        it != m_urlToDevice.end();
        )
    {
        delete it->second;
        m_urlToDevice.erase( it++ );
    }
}

QIODevice* QnExtIODeviceStorageResource::open( const QString& filePath, QIODevice::OpenMode openMode )
{
    Q_UNUSED(openMode)
    QMutexLocker lk( &m_mutex );

    std::map<QString, QIODevice*>::iterator it = m_urlToDevice.find( filePath );
    if( it == m_urlToDevice.end() )
        return NULL;
    QIODevice* dev = it->second;
    m_urlToDevice.erase( it );
    return dev;
}

bool QnExtIODeviceStorageResource::removeFile( const QString& path )
{
    QMutexLocker lk( &m_mutex );
    m_urlToDevice.erase( path );
    return true;
}

QFileInfoList QnExtIODeviceStorageResource::getFileList( const QString& /*dirName*/ )
{
    //TODO/IMPL
    return QFileInfoList();
}

bool QnExtIODeviceStorageResource::isFileExists( const QString& path )
{
    QMutexLocker lk( &m_mutex );
    return m_urlToDevice.find( path ) != m_urlToDevice.end();
}

qint64 QnExtIODeviceStorageResource::getFileSize( const QString& path ) const
{
    QMutexLocker lk( &m_mutex );

    std::map<QString, QIODevice*>::const_iterator it = m_urlToDevice.find( path );
    return it != m_urlToDevice.end() ? it->second->size() : 0;
}

void QnExtIODeviceStorageResource::registerResourceData( const QString& path, QIODevice* data )
{
    QMutexLocker lk( &m_mutex );

    std::pair<std::map<QString, QIODevice*>::iterator, bool>
        p = m_urlToDevice.insert( std::make_pair( path, data ) );
    if( !p.second )
    {
        delete p.first->second;
        p.first->second = data;
    }
}
