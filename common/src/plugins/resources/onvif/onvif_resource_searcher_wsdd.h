#ifndef onvif_resource_searcher_wsdd_h
#define onvif_resource_searcher_wsdd_h

#include "onvif_resource_information_fetcher.h"

struct wsdd__ProbeMatchesType;
struct wsdd__ProbeType;
struct wsa__EndpointReferenceType;
struct SOAP_ENV__Header;


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
    static const char WSDD_MULTICAST_ADDRESS[];

    OnvifResourceInformationFetcher& onvifFetcher;

public:
    static OnvifResourceSearcherWsdd& instance();

    void findResources(QnResourceList& result) const;

private:

    void findEndpoints(EndpointInfoHash& result) const;
    QStringList getAddrPrefixes(const QString& host) const;
    QString getAppropriateAddress(const wsdd__ProbeMatchesType* probeMatches, const QStringList& prefixes) const;
    QString getName(const wsdd__ProbeMatchesType* probeMatches) const;
    QString getManufacturer(const wsdd__ProbeMatchesType* probeMatches, const QString& name) const;
    QString getMac(const wsdd__ProbeMatchesType* probeMatches, const SOAP_ENV__Header* header) const;
    QString getEndpointAddress(const wsdd__ProbeMatchesType* probeMatches) const;
    void fillWsddStructs(wsdd__ProbeType& probe, wsa__EndpointReferenceType& endpoint) const;
    void printProbeMatches(const wsdd__ProbeMatchesType* probeMatches, const SOAP_ENV__Header* header) const;
    void addEndpointToHash(EndpointInfoHash& hash, const wsdd__ProbeMatchesType* probeMatches,
        const SOAP_ENV__Header* header, const QStringList& addrPrefixes, const QString& host) const;
};

#endif // onvif_resource_searcher_wsdd_h
