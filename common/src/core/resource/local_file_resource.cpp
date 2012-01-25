#include <QFileInfo>

#include "local_file_resource.h"
#include "core/dataprovider/single_shot_file_dataprovider.h"

#include <plugins/resources/archive/archive_stream_reader.h>
#include <plugins/resources/archive/avi_files/avi_archive_delegate.h>

QnLocalFileResource::QnLocalFileResource(const QString &filename)
    : QnResource()
{
    QFileInfo fi(filename);
    setName(fi.fileName());
    setUrl(fi.absoluteFilePath());

    addFlag(SINGLE_SHOT);
}

QString QnLocalFileResource::getUniqueId() const
{
    return getUrl();
}

QnAbstractStreamDataProvider* QnLocalFileResource::createDataProviderInternal(ConnectionRole /*role*/)
{
    return new CLSingleShotFileStreamreader(toSharedPointer());
}
