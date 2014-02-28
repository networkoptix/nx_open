/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef NETWORKOPTIXMODULEREVEALCOMMON_H
#define NETWORKOPTIXMODULEREVEALCOMMON_H

#include <QtNetwork/QHostAddress>
#include <QtCore/QMetaType>
#include <QtCore/QString>


static const char* NX_ENTERPISE_CONTROLLER_ID = "Enterprise Controller";
/*!
    This string represents client during search with NetworkOptixModuleFinder class.
    It may look strange, but "client.exe" is valid on linux too (VER_ORIGINALFILENAME_STR from version.h)
*/
static const char* NX_CLIENT_ID = "client.exe";
static const char* NX_MEDIA_SERVER_ID = "Media Server";

// declaring here to avoid GCC 'unused constant' warning spam
// also using them is much more convinient than call ::fromLatin1 every time
// TODO: #Elric Fix me if it is a bad idea
//static const QString nxEntControllerId = QLatin1String(NX_ENTERPISE_CONTROLLER_ID);
//static const QString nxEntControllerId = QLatin1String(NX_ENTERPISE_CONTROLLER_ID);
static const QString nxClientId = QLatin1String(NX_CLIENT_ID);
static const QString nxMediaServerId = QLatin1String(NX_MEDIA_SERVER_ID);

static const QHostAddress defaultModuleRevealMulticastGroup = QHostAddress(lit("239.255.11.11"));
static const unsigned int defaultModuleRevealMulticastGroupPort = 5007;

//!Number of simple functions to serialize simple types (local byte order is used)
namespace Serialization
{
    bool serialize( const quint64& val, quint8** const bufStart, const quint8* bufEnd );
    bool deserialize( quint64* const val, const quint8** const bufStart, const quint8* bufEnd );

    bool serialize( const qint64& val, quint8** const bufStart, const quint8* bufEnd );
    bool deserialize( qint64* const val, const quint8** const bufStart, const quint8* bufEnd );

    bool serialize( const quint16& val, quint8** const bufStart, const quint8* bufEnd );
    bool deserialize( quint16* const val, const quint8** const bufStart, const quint8* bufEnd );

    bool serialize( const quint32& val, quint8** const bufStart, const quint8* bufEnd );
    bool deserialize( quint32* const val, const quint8** const bufStart, const quint8* bufEnd );

    bool serialize( const QString& val, quint8** const bufStart, const quint8* bufEnd );
    bool deserialize( QString* const val, const quint8** const bufStart, const quint8* bufEnd );
}

//!This request is sent by host which tries to find other modules
class RevealRequest
{
public:
    bool serialize( quint8** const bufStart, const quint8* bufEnd );
    bool deserialize( const quint8** bufStart, const quint8* bufEnd );
};

typedef QMap<QString, QString> TypeSpecificParamMap;

Q_DECLARE_METATYPE(TypeSpecificParamMap);

//!Sent in response to RevealRequest by module which reveals itself
class RevealResponse
{
public:
    //!Name of module (enterprise controller, media server, etc...)
    QString type;
    QString version;
    QString customization;
    QString name;
    //!random string, unique for particular module instance
    QString seed;
    TypeSpecificParamMap typeSpecificParameters;

    RevealResponse();

    bool serialize( quint8** const bufStart, const quint8* bufEnd );
    bool deserialize( const quint8** bufStart, const quint8* bufEnd );
};

#endif  //NETWORKOPTIXMODULEREVEALCOMMON_H
