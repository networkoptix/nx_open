// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ext_iodevice_storage.h"

#include <nx/utils/thread/mutex.h>

QnExtIODeviceStorageResource::QnExtIODeviceStorageResource():
    base_type(),
    m_capabilities(0)
{
    m_capabilities |= QnAbstractStorageResource::cap::ListFile;
    m_capabilities |= QnAbstractStorageResource::cap::ReadFile;
}

QnExtIODeviceStorageResource::~QnExtIODeviceStorageResource()
{
    for( std::map<QString, QIODevice*>::iterator
        it = m_urlToDevice.begin();
        it != m_urlToDevice.end();
        )
    {
        if (m_isIoDeviceOwner)
            delete it->second;
        m_urlToDevice.erase( it++ );
    }
}

int QnExtIODeviceStorageResource::getCapabilities() const
{
    return m_capabilities;
}

QIODevice* QnExtIODeviceStorageResource::openInternal( const QString& filePath, QIODevice::OpenMode /*openMode*/ )
{
    NX_MUTEX_LOCKER lk( &m_mutex );

    std::map<QString, QIODevice*>::iterator it = m_urlToDevice.find( filePath );
    if( it == m_urlToDevice.end() )
        return NULL;
    QIODevice* dev = it->second;

    if (m_isIoDeviceOwner)
        m_urlToDevice.erase( it );
    return dev;
}

bool QnExtIODeviceStorageResource::removeFile( const QString& path )
{
    NX_MUTEX_LOCKER lk( &m_mutex );
    m_urlToDevice.erase( path );
    return true;
}

QnAbstractStorageResource::FileInfoList QnExtIODeviceStorageResource::getFileList( const QString& /*dirName*/ )
{
    //TODO/IMPL
    return QnAbstractStorageResource::FileInfoList();
}

bool QnExtIODeviceStorageResource::isFileExists( const QString& path )
{
    NX_MUTEX_LOCKER lk( &m_mutex );
    return m_urlToDevice.find( path ) != m_urlToDevice.end();
}

qint64 QnExtIODeviceStorageResource::getFileSize( const QString& path ) const
{
    NX_MUTEX_LOCKER lk( &m_mutex );

    std::map<QString, QIODevice*>::const_iterator it = m_urlToDevice.find( path );
    return it != m_urlToDevice.end() ? it->second->size() : 0;
}

void QnExtIODeviceStorageResource::registerResourceData( const QString& path, QIODevice* data )
{
    NX_MUTEX_LOCKER lk( &m_mutex );

    std::pair<std::map<QString, QIODevice*>::iterator, bool>
        p = m_urlToDevice.insert( std::make_pair( path, data ) );
    if( !p.second )
    {
        if (m_isIoDeviceOwner)
            delete p.first->second;
        p.first->second = data;
    }
}

void QnExtIODeviceStorageResource::setIsIoDeviceOwner(bool isIoDeviceOwner)
{
    m_isIoDeviceOwner = isIoDeviceOwner;
}
