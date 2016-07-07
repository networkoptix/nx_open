
#include "cf_url.h"

#if defined(Q_OS_MAC)

#include <QtCore/QString>
#include <platform/core_foundation_mac/cf_string.h>

cf::QnCFUrl::QnCFUrl() :
    base_type()
{}

cf::QnCFUrl::~QnCFUrl()
{}

cf::QnCFUrl cf::QnCFUrl::createFileUrl(const QString& fileName)
{
    QnCFUrl result;

    result.reset(CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
        QnCFString(fileName).ref(), kCFURLPOSIXPathStyle, false));
    return result;
}

#endif
