#ifndef directory_browser_h_1708
#define directory_browser_h_1708

#include "../resourcemanagment/resourceserver.h"

class QnResourceDirectoryBrowser : public QnAbstractFileResourceSearcher
{
    QnResourceDirectoryBrowser();

public:
    virtual ~QnResourceDirectoryBrowser();

    QnResourcePtr createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters);
    bool isResourceTypeSupported(const QnId& resourceTypeId) const;

    static QnResourceDirectoryBrowser &instance();

    virtual QString manufacture() const;
    virtual QnResourceList findResources();

    // creates an instance of proper resource from file
    virtual QnResourcePtr checkFile(const QString &filename);
    virtual QnResourceList checkFiles(const QStringList files);

protected:
    static QnResourcePtr createArchiveResource(const QString& xfile);
    QnResourceList findResources(const QString &directory);
};

#endif //directory_browser_h_1708
