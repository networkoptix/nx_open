#ifndef directory_browser_h_1708
#define directory_browser_h_1708

#include "resource/resource.h"
#include "resourcecontrol/abstract_resource_searcher.h"




class QnResourceDirectoryBrowser : public QnAbstractFileResourceSearcher
{
    QnResourceDirectoryBrowser();
public:
	virtual ~QnResourceDirectoryBrowser();

    static QnResourceDirectoryBrowser& instance();

    virtual QString name() const;
    virtual QnResourceList findDevices();

    // creates an instance of proper resource from file 
    virtual QnResource* checkFile(const QString& filename);

protected:
    QnResourceList findDevices(const QString& directory);
private:
    static QStringList subDirList(const QString& abspath);
 

};

#endif //directory_browser_h_1708
