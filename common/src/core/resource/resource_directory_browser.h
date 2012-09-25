#ifndef QN_RESOURCE_DIRECTORY_BROWSER_H
#define QN_RESOURCE_DIRECTORY_BROWSER_H

#include "../resource_managment/resource_searcher.h"

class QnResourceDirectoryBrowser : public QnAbstractFileResourceSearcher
{
public:
    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    static QnResourceDirectoryBrowser &instance();

    virtual QString manufacture() const override;
    virtual QnResourceList findResources() override;

    virtual QnResourcePtr checkFile(const QString &filename) const override;

    // Lical files search only once. Use cleanup before search to re-search files again
    void cleanup();
protected:
    bool m_resourceReady;

    QnResourceDirectoryBrowser();

    static QnResourcePtr createArchiveResource(const QString& xfile);

    QnResourceList findResources(const QString &directory);
};

#endif //QN_RESOURCE_DIRECTORY_BROWSER_H
