#ifndef QN_RESOURCE_DIRECTORY_BROWSER_H
#define QN_RESOURCE_DIRECTORY_BROWSER_H

#include "../resourcemanagment/resource_searcher.h"

class QnResourceDirectoryBrowser : public QnAbstractFileResourceSearcher
{
    QnResourceDirectoryBrowser();

public:
    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    static QnResourceDirectoryBrowser &instance();

    virtual QString manufacture() const;
    virtual QnResourceList findResources();

    virtual QnResourcePtr checkFile(const QString &filename) const;

protected:
    static QnResourcePtr createArchiveResource(const QString& xfile);
    QnResourceList findResources(const QString &directory);
};

#endif //QN_RESOURCE_DIRECTORY_BROWSER_H
