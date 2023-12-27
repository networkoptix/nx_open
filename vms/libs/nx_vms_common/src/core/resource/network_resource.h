// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/media_resource.h>
#include <nx/utils/mac_address.h>
#include <nx/utils/url.h>
#include <nx/utils/value_cache.h>

#include "resource.h"

class QAuthenticator;

class NX_VMS_COMMON_API QnNetworkResource: public QnMediaResource
{
    Q_OBJECT

    using base_type = QnResource;
public:
    enum NetworkStatusFlag
    {
        BadHostAddr = 0x01,
        Ready = 0x04
    };
    Q_DECLARE_FLAGS(NetworkStatus, NetworkStatusFlag)

    QnNetworkResource();
    virtual ~QnNetworkResource();

    virtual void setUrl(const QString& url) override;

    virtual QString getHostAddress() const;
    virtual void setHostAddress(const QString &ip);

    nx::utils::MacAddress getMAC() const;
    void setMAC(const nx::utils::MacAddress &mac);

    virtual QString getPhysicalId() const;
    void setPhysicalId(const QString& physicalId);

    void setAuth(const QString &user, const QString &password);
    void setAuth(const QAuthenticator &auth);

    void setDefaultAuth(const QString &user, const QString &password);
    void setDefaultAuth(const QAuthenticator &auth);

    /**
     * Parse user and password from the colon-delimited string.
     */
    static QAuthenticator parseAuth(const QString& value);

    QAuthenticator getAuth() const;
    QAuthenticator getDefaultAuth() const;

    // if reader will find out that authentication is required => setAuthenticated(false) must be called
    bool isAuthenticated() const;
    void setAuthenticated(bool auth);

    virtual int httpPort() const;
    virtual void setHttpPort( int newPort );

    /*!
        By default, it is rtsp port (554)
    */
    virtual int mediaPort() const;
    void setMediaPort(int value);

    void addNetworkStatus(NetworkStatus status);
    void removeNetworkStatus(NetworkStatus status);
    bool checkNetworkStatus(NetworkStatus status) const;
    void setNetworkStatus(NetworkStatus status);

    // this value is updated by discovery process
    QDateTime getLastDiscoveredTime() const;
    void setLastDiscoveredTime(const QDateTime &time);

    // all data readers and any sockets will use this number as timeout value in ms
    void setNetworkTimeout(unsigned int timeout);
    virtual unsigned int getNetworkTimeout() const;

    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

    // in some cases I just want to update couple of field from just discovered resource
    virtual bool mergeResourcesIfNeeded(const QnNetworkResourcePtr &source);

    virtual int getChannel() const;

    //!Returns true if camera is accessible
    /*!
        Default implementation just establishes connection to \a getHostAddress() : \a httpPort()
        \todo #akolesnikov This method is used in diagnostics only. Throw it away and use \a QnNetworkResource::checkIfOnlineAsync instead
    */
    virtual bool ping();
    //!Checks if camera is online
    /*!
        \param completionHandler Invoked on check completion. Check result is passed to the functor
        \return true if async operation has been started. false otherwise
        \note Implementation MUST check not only camera address:port accessibility, but also check some unique parameters of camera
        \note Default implementation returns false
    */
    virtual void checkIfOnlineAsync( std::function<void(bool)> completionHandler );

    static QnUuid physicalIdToId(const QString& uniqId);

    virtual QString idForToStringFromPtr() const override;

    static QString mediaPortKey();

private:
    bool m_authenticated = true;

    nx::utils::MacAddress m_macAddress;
    QString m_physicalId;

    NetworkStatus m_networkStatus = {};

    unsigned int m_networkTimeout = 1000 * 10;

    // Initialized in cpp to avoid transitional includes.
    int m_httpPort;

    bool m_probablyNeedToUpdateStatus = false;

    QDateTime m_lastDiscoveredTime;

    nx::utils::CachedValue<QString> m_cachedHostAddress;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnNetworkResource::NetworkStatus)
