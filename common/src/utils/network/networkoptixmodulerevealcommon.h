/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef NETWORKOPTIXMODULEREVEALCOMMON_H
#define NETWORKOPTIXMODULEREVEALCOMMON_H

#include <QtNetwork/QHostAddress>
#include <QtCore/QMetaType>

#include <utils/network/module_information.h>

/*!
    This string represents client during search with NetworkOptixModuleFinder class.
    It may look strange, but "client.exe" is valid on linux too (VER_ORIGINALFILENAME_STR from app_info.h)
*/
static const QString nxClientId = lit("client.exe");
static const QString nxMediaServerId = lit("Media Server");
static const QString nxECId = lit("Enterprise Controller");

static const QHostAddress defaultModuleRevealMulticastGroup = QHostAddress(lit("239.255.11.11"));
static const quint16 defaultModuleRevealMulticastGroupPort = 5007;

//!This request is sent by host which tries to find other modules
class RevealRequest
{
public:
    bool serialize(quint8 ** const bufStart, const quint8 *bufEnd);
    static bool isValid(const quint8 *bufStart, const quint8 *bufEnd);
};

typedef QMap<QString, QString> TypeSpecificParamMap;

Q_DECLARE_METATYPE(TypeSpecificParamMap)

//!Sent in response to RevealRequest by module which reveals itself
struct RevealResponse : public QnModuleInformation {
public:
    RevealResponse() {}
    RevealResponse(const QnModuleInformation &other);
    QByteArray serialize();
    bool deserialize(const quint8 *bufStart, const quint8 *bufEnd);
};

#endif  //NETWORKOPTIXMODULEREVEALCOMMON_H
