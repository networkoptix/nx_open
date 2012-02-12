#include "abstract_archive_resource.h"


QnAbstractArchiveResource::QnAbstractArchiveResource()
{
	addFlags(QnResource::ARCHIVE);
}

QnAbstractArchiveResource::~QnAbstractArchiveResource()
{

}

QString QnAbstractArchiveResource::getUniqueId() const
{
    return getUrl();
}
