#ifndef onvif_resource_searcher_wsdd_h
#define onvif_resource_searcher_wsdd_h

#include "onvif_resource_information_fetcher.h"

struct wsdd__ProbeMatchesType;
struct wsdd__ProbeType;
struct wsa__EndpointReferenceType;
struct SOAP_ENV__Header;

typedef QSharedPointer<QUdpSocket> QUdpSocketPtr;


class OnvifResourceSearcherWsdd
{
    OnvifResourceSearcherWsdd();

    static QString& LOCAL_ADDR;
    static const char SCOPES_NAME_PREFIX[];
    static const char SCOPES_HARDWARE_PREFIX[];
    static const char PROBE_TYPE[];
    static const char WSA_ADDRESS[];
    static const char WSDD_ADDRESS[];
    static const char WSDD_ACTION[];
    static const char WSDD_GSOAP_MULTICAST_ADDRESS[];

    OnvifResourceInformationFetcher& m_onvifFetcher;
    //mutable QHash<QString, QUdpSocketPtr> m_recvSocketList;
    //mutable QMutex m_mutex;

public:
    static OnvifResourceSearcherWsdd& instance();

    void findResources(QnResourceList& result) const;

    void pleaseStop();
private:

    //void updateInterfacesListenSockets() const;
    //void findHelloEndpoints(EndpointInfoHash& result) const;
    void findEndpoints(EndpointInfoHash& result) const;
    QStringList getAddrPrefixes(const QString& host) const;
    void fillWsddStructs(wsdd__ProbeType& probe, wsa__EndpointReferenceType& endpoint) const;

    template <class T> QString getAppropriateAddress(const T* source, const QStringList& prefixes) const;
    template <class T> QString getName(const T* source) const;
    template <class T> QString getManufacturer(const T* source, const QString& name) const;
    template <class T> QString getMac(const T* source, const SOAP_ENV__Header* header) const;
    template <class T> QString getEndpointAddress(const T* source) const;
    template <class T> void printProbeMatches(const T* source, const SOAP_ENV__Header* header) const;
    template <class T> void addEndpointToHash(EndpointInfoHash& hash, const T* probeMatches,
        const SOAP_ENV__Header* header, const QStringList& addrPrefixes, const QString& host) const;
private:
    bool m_shouldStop;
};

#endif // onvif_resource_searcher_wsdd_h
