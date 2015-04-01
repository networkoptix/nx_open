/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef NETWORKOPTIXMODULEREVEALCOMMON_H
#define NETWORKOPTIXMODULEREVEALCOMMON_H

#include <QtNetwork/QHostAddress>
#include <QtCore/QMetaType>

#include <utils/network/module_information.h>

//!This request is sent by host which tries to find other modules
class RevealRequest
{
public:
    bool serialize(quint8 ** const bufStart, const quint8 *bufEnd);
    bool deserialize(const quint8 **bufStart, const quint8 *bufEnd);
};

typedef QMap<QString, QString> TypeSpecificParamMap;

Q_DECLARE_METATYPE(TypeSpecificParamMap)

//!Sent in response to RevealRequest by module which reveals itself
class RevealResponse
{
public:
    //!Name of module (server, client, etc...)
    QString type;
    QString version;
    QString systemInformation;
    QString customization;
    QString name;
    QString moduleName;
    //!random string, unique for particular module instance
    QnUuid seed;
    quint16 port;
    bool sslAllowed;
    QStringList remoteAddresses;
    QByteArray authHash;
    int protoVersion;

    RevealResponse();
    RevealResponse(const QnModuleInformation &moduleInformation);

    QnModuleInformation toModuleInformation() const;

    bool serialize(quint8 ** const bufStart, const quint8 *bufEnd);
    bool deserialize(const quint8 **bufStart, const quint8 *bufEnd);
};

#endif  //NETWORKOPTIXMODULEREVEALCOMMON_H
