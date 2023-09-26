// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qfile_metadata_binary_file.h"

namespace nx::vms::metadata {

void QFileMetadataBinaryFile::close()
{
    m_file.close();
}

void QFileMetadataBinaryFile::setFileName(const QString& fName)
{
    m_file.setFileName(fName);
}

bool QFileMetadataBinaryFile::openReadOnly()
{
    return m_file.open(QFile::ReadOnly);
}

bool QFileMetadataBinaryFile::openRW()
{
   return m_file.open(QFile::ReadWrite);
}

bool QFileMetadataBinaryFile::resize(int64_t sz)
{
    return m_file.resize(sz);
}

bool QFileMetadataBinaryFile::seek(int64_t offset)
{
    return m_file.seek(offset);
}

int64_t QFileMetadataBinaryFile::read(char* data, int64_t maxSize)
{
    return m_file.read(data, maxSize);
}

QString QFileMetadataBinaryFile::fileName() const
{
    return m_file.fileName();
}

bool QFileMetadataBinaryFile::isOpen() const
{
    return m_file.isOpen();
}

int64_t QFileMetadataBinaryFile::size() const
{
    return m_file.size();
}

int64_t QFileMetadataBinaryFile::write(const char* buffer, qint64 count)
{
    return m_file.write(buffer, count);
}

bool QFileMetadataBinaryFile::flush()
{
    return m_file.flush();
}

} // namespace nx::vms::metadata
