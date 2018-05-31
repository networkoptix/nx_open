#ifndef onvif_resource_searcher_wsdd_h
#define onvif_resource_searcher_wsdd_h

#ifdef ENABLE_ONVIF

#include "onvif_resource_information_fetcher.h"

#include <map>
#include <set>
#include <vector>
#include <memory>

#include <QtCore/QString>

#include <nx/network/nettools.h>

#include "onvif/soapwsddProxy.h"

class UDPSocket;

struct wsdd__ProbeMatchesType;
struct wsdd__ProbeType;
struct wsa__EndpointReferenceType;
struct SOAP_ENV__Header;

typedef QSharedPointer<QUdpSocket> QUdpSocketPtr;

struct CameraInfo: public EndpointAdditionalInfo
{
    QString onvifUrl;

    CameraInfo() {}

    CameraInfo(const QString& newOnvifUrl, const EndpointAdditionalInfo& additionalInfo):
        EndpointAdditionalInfo(additionalInfo),
        onvifUrl(newOnvifUrl)
    {

    }

    CameraInfo(const CameraInfo& src) :
        EndpointAdditionalInfo(src),
        onvifUrl(src.onvifUrl)
    {

    }
};

class OnvifResourceSearcherWsdd
{
    static QString LOCAL_ADDR;
    static const char SCOPES_NAME_PREFIX[];
    static const char SCOPES_LOCATION_PREFIX[];
    static const char SCOPES_HARDWARE_PREFIX[];
    static const char PROBE_TYPE[];
    static const char WSA_ADDRESS[];
    static const char WSDD_ADDRESS[];
    static const char WSDD_ACTION[];
    static const char WSDD_GSOAP_MULTICAST_ADDRESS[];

    static const QString OBTAIN_MAC_FROM_MULTICAST_PARAM_NAME;

    OnvifResourceInformationFetcher* m_onvifFetcher;
    //mutable QHash<QString, QUdpSocketPtr> m_recvSocketList;
    //mutable QnMutex m_mutex;
public:
    enum ObtainMacFromMulticast {Auto, Always, Never};

public:
    OnvifResourceSearcherWsdd(OnvifResourceInformationFetcher* informationFetcher);
    virtual ~OnvifResourceSearcherWsdd();

    void findResources(QnResourceList& result, DiscoveryMode discoveryMode);

    void pleaseStop();

private:

    //void updateInterfacesListenSockets() const;
    //void findHelloEndpoints(EndpointInfoHash& result) const;
    void findEndpoints(EndpointInfoHash& result);
    QStringList getAddrPrefixes(const QString& host) const;
    void fillWsddStructs(wsdd__ProbeType& probe, wsa__EndpointReferenceType& endpoint) const;

    //If iface is not null, the function will perform multicast search, otherwise - unicast
    //iface and camAddr MUST NOT be 0 at the same time
    void findEndpointsImpl( EndpointInfoHash& result, const nx::network::QnInterfaceAndAddr& iface ) const;

    template <class T> QString getAppropriateAddress(const T* source, const QStringList& prefixes) const;
    template <class T> QString extractScope(const T* source, const QString& pattern) const;
    template <class T> QString getManufacturer(const T* source, const QString& name) const;

    template <typename T>
    std::set<QString> additionalManufacturers(
        const T* source,
        const std::vector<QString> additionalManufacturerPrefixes) const;

    template <class T> QString getMac(const T* source, const SOAP_ENV__Header* header) const;
    template <class T> QString getEndpointAddress(const T* source) const;
    template <class T> void printProbeMatches(const T* source, const SOAP_ENV__Header* header) const;
    template <class T> void addEndpointToHash(EndpointInfoHash& hash, const T* probeMatches,
        const SOAP_ENV__Header* header, const QStringList& addrPrefixes, const QString& host) const;

private:
    class ProbeContext
    {
    public:
        std::unique_ptr<nx::network::AbstractDatagramSocket> sock;
        wsddProxy soapWsddProxy;
        wsdd__ProbeType wsddProbe;
        wsa__EndpointReferenceType replyTo;

        ProbeContext();
        ~ProbeContext();

        void initializeSoap();

    private:
        ProbeContext( const ProbeContext& );
        ProbeContext& operator=( const ProbeContext& );
    };

    bool m_shouldStop;
    mutable std::map<QString, ProbeContext*> m_ifaceToSock;
    bool m_isFirstSearch;

    bool sendProbe( const nx::network::QnInterfaceAndAddr& iface );
    bool readProbeMatches( const nx::network::QnInterfaceAndAddr& iface, EndpointInfoHash& result );
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(OnvifResourceSearcherWsdd::ObtainMacFromMulticast)
QN_FUSION_DECLARE_FUNCTIONS(OnvifResourceSearcherWsdd::ObtainMacFromMulticast, (metatype)(numeric)(lexical))

#endif //ENABLE_ONVIF

#endif // onvif_resource_searcher_wsdd_h
