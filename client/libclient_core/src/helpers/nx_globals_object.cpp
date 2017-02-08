#include "nx_globals_object.h"

NxGlobalsObject::NxGlobalsObject(QObject* parent):
    QObject(parent)
{
}

QnUrlHelper NxGlobalsObject::url(const QUrl& url) const
{
    return QnUrlHelper(url);
}

QnSoftwareVersion NxGlobalsObject::softwareVersion(const QString& version) const
{
    return QnSoftwareVersion(version);
}
