#ifndef QN_RESOURCE_SEARCHER_H
#define QN_RESOURCE_SEARCHER_H

#include <QtCore/QMutex>
#include <QtCore/QStringList>

#include <core/resource/resource_factory.h>

class QUrl;
class QAuthenticator;

/**
 * Interface for resource searcher plugins.
 */
class QnAbstractResourceSearcher : public QnResourceFactory
{
protected:
    QnAbstractResourceSearcher();

public:
    virtual ~QnAbstractResourceSearcher();

    /**
     * Enables or disables this searcher.
     * 
     * \param use                       Whether this searcher should be used.
     */
    void setShouldBeUsed(bool use);

    /**
     * \returns                         Whether this searcher should be used.
     */
    bool shouldBeUsed() const;

    /**
     * Searches for resources.
     * 
     * \returns                         List of resources found.
     */
    QnResourceList search();

    /**
     * Search for resources may take time. This function can be used to
     * stop resource search prematurely. 
     */
    virtual void pleaseStop();

    bool isLocal() const;

    void setLocal(bool l);

    /**
     * \param resourceTypeId            Identifier of the type to check.
     * \returns                         Whether this factory can be used to create resources of the given type.
     */
    virtual bool isResourceTypeSupported(QUuid resourceTypeId) const;

    /**
     * \returns                         Name of the manufacturer for the resources this searcher adds. 
     *                                  For example, 'AreconVision' or 'IQInVision'.
     */ 
    virtual QString manufacture() const = 0;


    /** \returns                        Whether this factory generates virtual resources such as desktop cameras. */
    virtual bool isVirtualResource() const { return false; }
protected:
    /**
     * This is the actual function that searches for resources. 
     * To be implemented in derived classes.
     *
     * \returns                         List of resources found.
     */
    virtual QnResourceList findResources() = 0;

    /**
     * This function is to be used in derived classes. If this function returns
     * true, then <tt>findResources</tt> should return as soon as possible.
     * Its results will most probably be discarded.
     *
     * \returns                         Whether this resource searcher should
     *                                  stop searching as soon as possible.
     */
    bool shouldStop() const;

private:
    bool m_shouldbeUsed;
    bool m_localResources;
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
    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) = 0;
};

//=====================================================================

class QnAbstractFileResourceSearcher : virtual public QnAbstractResourceSearcher // TODO: #Elric why virtual inheritance?
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

#endif //QN_RESOURCE_SEARCHER_H
