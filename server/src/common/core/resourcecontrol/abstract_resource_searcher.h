#ifndef device_server_h_1658
#define device_server_h_1658

#include "resource/resource.h"

// this is an interface for resource searcher plug in
class QnAbstractResourceSearcher
{
protected:
    QnAbstractResourceSearcher();
public:
	virtual ~QnAbstractResourceSearcher();

	// return the name of the server; normaly this is manufacture name; like AreconVision or IQInVision
	virtual QString name() const = 0;

    // some searchers might be enabled or disabled
    void setSholdBeUsed(bool use);

    // returns true if should be used;
    bool sholdBeUsed() const;

	// returns all available devices 
	virtual QnResourceList findResources() = 0;
    
    // is some cases search might take time 
    //if pleaseStop is called search will be interrupted 
    void pleaseStop();

protected:

    bool shouldStop() const;

protected:
    bool m_sholudbeUsed;
    volatile bool m_shouldStop;
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
    QnAbstractFileResourceSearcher(){};
public:

    // sets the Path check List
    void setPathCheckList(const QStringList& paths);
    void clearPathCheckList();

    virtual QnResourcePtr checkFile(const QString& filename) = 0;

protected:
    QStringList m_pathListToCheck;
};



#endif //device_server_h_1658
