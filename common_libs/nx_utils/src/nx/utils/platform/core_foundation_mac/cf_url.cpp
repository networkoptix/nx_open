#include "cf_url.h"

#include <QtCore/QString>
#include <nx/utils/platform/core_foundation_mac/cf_string.h>

cf::QnCFUrl::QnCFUrl()
    :
    base_type()
{
}

cf::QnCFUrl::~QnCFUrl()
{
}

cf::QnCFUrl cf::QnCFUrl::createFileUrl(const QString& fileName)
{
    QnCFUrl result;

    result.reset(CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
        QnCFString(fileName).ref(), kCFURLPOSIXPathStyle, false));

    return result;
}

QString cf::QnCFUrl::toString(const CFURLRef ref)
{
    if (!ref)
        return QString();

    return cf::QnCFString::makeOwned(CFURLGetString(ref)).toString();
}

