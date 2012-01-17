#ifndef device_server_h_1658
#define device_server_h_1658

#include <QtNetwork/QHostAddress>
#include "core/resource/resource.h"




// this is an interface for resource searcher plug in
class QnAbstractResourceSearcher : public QnResourceFactory
{
protected:
    QnAbstractResourceSearcher();
public:
    virtual ~QnAbstractResourceSearcher();

    // return the manufacture of the server; normaly this is manufacture manufacture; like AreconVision or IQInVision
    virtual QString manufacture() const = 0;

    // some searchers might be enabled or disabled
    void setShouldBeUsed(bool use);

    // returns true if should be used;
    bool shouldBeUsed() const;

    // returns all available devices
    virtual QnResourceList findResources() = 0;

    virtual bool isResourceTypeSupported(const QnId& resourceTypeId) const;

    // is some cases search might take time
    //if pleaseStop is called search will be interrupted
    void pleaseStop();

    bool isLocal() const;
    void setLocal(bool l);

protected:

    bool shouldStop() const;

protected:
    bool m_shouldbeUsed;
    volatile bool m_shouldStop;

private:
    bool m_localResources;
};

//=====================================================================
class QnAbstractNetworkResourceSearcher : virtual public QnAbstractResourceSearcher
{
protected:
    QnAbstractNetworkResourceSearcher (){};
public:

    // checks this QHostAddress and creates a QnResource in case of success
    // this function is designed for manual resource addition
    virtual QnResourcePtr checkHostAddr(QHostAddress addr) = 0;
};

//=====================================================================

class QnAbstractFileResourceSearcher : virtual public QnAbstractResourceSearcher
{
protected:
    QnAbstractFileResourceSearcher() {}

public:
    QStringList getPathCheckList() const;
    void setPathCheckList(const QStringList& paths);
    inline void clearPathCheckList()
    { setPathCheckList(QStringList()); }

    // creates an instance of proper resource from file
    virtual QnResourcePtr checkFile(const QString &filename) const = 0;
    QnResourceList checkFiles(const QStringList &files) const;

protected:
    mutable QMutex m_mutex;
    QStringList m_pathListToCheck;
};

#endif //device_server_h_1658
