#pragma once

#include <atomic>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>
#include <QtCore/QStringList>

#include <core/resource/resource_factory.h>
#include <common/common_module_aware.h>

class QAuthenticator;

enum class DiscoveryMode
{
    fullyEnabled,
    //!In this mode only known cameras are searched. Unknown cameras are ignored
    partiallyEnabled,
    //!No auto camera discovery is done
    disabled
};

/**
 * Interface for resource searcher plugins.
 */
class QnAbstractResourceSearcher: public QnResourceFactory, public /*mixin*/ QnCommonModuleAware
{
protected:
    explicit QnAbstractResourceSearcher(QnCommonModule* commonModule) noexcept;

public:
    virtual ~QnAbstractResourceSearcher() = default;

    //!Enables/disables camera discovery
    void setDiscoveryMode(DiscoveryMode mode) noexcept;
    DiscoveryMode discoveryMode() const noexcept;

    /**
     * Some searchers should be run first and in sequential order.
     * It's needed because some devices just are not able to handle
     * many search request at the same time. If such a searcher returns
     * not empty list of found resources then search process stops and no searcher
     * checks this url after that.
     *
     * Sequential searcher should be as fast as possible in order to not increase search
     * iteration time very much.
     *
     */
    virtual bool isSequential() const {return false;}

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

    bool isLocal() const noexcept;

    void setLocal(bool l) noexcept;

    /**
     * \param resourceTypeId            Identifier of the type to check.
     * \returns                         Whether this factory can be used to create resources of the given type.
     */
    virtual bool isResourceTypeSupported(QnUuid resourceTypeId) const;

    /**
     * \returns                         Name of the manufacturer for the resources this searcher adds.
     *                                  For example, 'AreconVision' or 'IQInVision'.
     */
    virtual QString manufacturer() const = 0;

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
    bool shouldStop() const noexcept;

private:
    DiscoveryMode m_discoveryMode;
    bool m_isLocal;
    std::atomic<bool> m_shouldStop;
};

// =====================================================================
class QnAbstractNetworkResourceSearcher: public QnAbstractResourceSearcher
{
protected:
    QnAbstractNetworkResourceSearcher(QnCommonModule* commonModule);
public:
    // TODO: #wearable use QnResourceList for return type!
    // checks this QHostAddress and creates a QnResource in case of success
    // this function is designed for manual resource addition
    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) = 0;
};
