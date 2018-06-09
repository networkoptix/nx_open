/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef NETWORKOPTIXMODULEREVEALCOMMON_H
#define NETWORKOPTIXMODULEREVEALCOMMON_H

#include <QtNetwork/QHostAddress>
#include <QtCore/QMetaType>

#include "module_information.h"

#include <nx/vms/api/types_fwd.h>

//!This request is sent by host which tries to find other modules
class RevealRequest
{
public:
    RevealRequest(const QnUuid& moduleGuid, nx::vms::api::PeerType peerType);

    QByteArray serialize();
    static bool isValid(const quint8 *bufStart, const quint8 *bufEnd);
private:
    const QnUuid m_moduleGuid;
    const nx::vms::api::PeerType m_peerType;
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
