#ifndef directory_browser_h_1708
#define directory_browser_h_1708

#include "../resourcemanagment/resourceserver.h"

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

#endif //directory_browser_h_1708
