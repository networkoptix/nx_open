// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include <QtCore/QStringList>
#include <QtNetwork/QAuthenticator>

#include <core/resource/resource_factory.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>
#include <nx/vms/common/system_context_aware.h>

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
class NX_VMS_COMMON_API QnAbstractResourceSearcher:
    public QnResourceFactory,
    public nx::vms::common::SystemContextAware
{
protected:
    explicit QnAbstractResourceSearcher(nx::vms::common::SystemContext* systemContext) noexcept;

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
    virtual bool isResourceTypeSupported(nx::Uuid resourceTypeId) const;

    /**
     * \returns                         Name of the manufacturer for the resources this searcher adds.
     *                                  For example, 'ArecontVision' or 'IQInVision'.
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
class NX_VMS_COMMON_API QnAbstractNetworkResourceSearcher:
    virtual public QnAbstractResourceSearcher
{
protected:
    QnAbstractNetworkResourceSearcher(nx::vms::common::SystemContext* systemContext);

public:
    struct AddressCheckParams
    {
        nx::utils::Url url;
        QAuthenticator auth;
        bool doMultichannelCheck = false;
    };

public:
    // Check host, create and return Resources in case of success
    virtual QList<QnResourcePtr> checkAddress(AddressCheckParams params)
    {
        // do nothing else by default
        return checkHostAddr(params);
    }

    // Searcher is able to find non-network resources.
    virtual bool canFindLocalResources() const { return false; }

    // Returns whether provided URL scheme is supported by searcher
    virtual bool isUrlSchemeSupported(const QString& scheme) const = 0;

    /** Some resource search drivers can only see resource via 'http' scheme in spite of resource accepting 'https' connections only.
     * It is caused by camera bugs. They just continue to report 'http' scheme in multicast messages.
     * This helper function checks whether it's needed to update URL to 'https' or not.
     */

    static nx::utils::Url updateUrlToHttpsIfNeed(const nx::utils::Url& foundUrl);

protected:
    // checks this host and creates a QnResource in case of success
    // this function is designed for manual resource addition
    virtual QList<QnResourcePtr> checkHostAddr(const AddressCheckParams& params) = 0;
};

// =====================================================================
class NX_VMS_COMMON_API QnAbstractHttpResourceSearcher:
    virtual public QnAbstractNetworkResourceSearcher
{
protected:
    QnAbstractHttpResourceSearcher(nx::vms::common::SystemContext* systemContext);
    virtual int portForScheme(const QString& scheme) const;

public:
    virtual QList<QnResourcePtr> checkAddress(AddressCheckParams params) override;

    virtual bool isUrlSchemeSupported(const QString& scheme) const override;
};
