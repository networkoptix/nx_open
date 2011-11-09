#include "abstract_archive_resource.h"


QnAbstractArchiveResource::QnAbstractArchiveResource()
{
	addFlag(QnResource::ARCHIVE);
}

QnAbstractArchiveResource::~QnAbstractArchiveResource()
{

}

QString QnAbstractArchiveResource::getUniqueId() const
{
    return getUrl();
}
