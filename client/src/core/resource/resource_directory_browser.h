#ifndef QN_RESOURCE_DIRECTORY_BROWSER_H
#define QN_RESOURCE_DIRECTORY_BROWSER_H

#include "core/resource_management/resource_searcher.h"

class QnResourceDirectoryBrowser : public QnAbstractFileResourceSearcher
{
public:
    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    static QnResourceDirectoryBrowser &instance();

    virtual QString manufacture() const override;
    virtual QnResourceList findResources() override;

    virtual QnResourcePtr checkFile(const QString &filename) const override;

    // Lical files search only once. Use cleanup before search to re-search files again
    void cleanup();

    static QnLayoutResourcePtr layoutFromFile(const QString& xfile);

protected:
    bool m_resourceReady;

    QnResourceDirectoryBrowser();

    static QnResourcePtr createArchiveResource(const QString& xfile);

    void findResources(const QString &directory, QnResourceList *result);
};

#endif //QN_RESOURCE_DIRECTORY_BROWSER_H
