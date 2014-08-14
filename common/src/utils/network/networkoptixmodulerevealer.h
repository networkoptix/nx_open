/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef NETWORKOPTIXMODULEREVEALER_H
#define NETWORKOPTIXMODULEREVEALER_H

#include <vector>

#include <QtCore/QThread>

#include "networkoptixmodulerevealcommon.h"
#include "aio/pollset.h"
#include "../common/long_runnable.h"


class UDPSocket;

//!Makes module to be located in network by \a NetworkOptixModuleFinder instances
/*!
    Joins specified multicast group on every local ipv4-address and responses to received requests
*/
class NetworkOptixModuleRevealer
:
    public QnLongRunnable
{
public:
    /*!
        \param multicastGroupAddress Group to join
        \param multicastGroupPort Port to listen
    */
    NetworkOptixModuleRevealer(
        const QString& moduleType,
        const QString& moduleVersion,
        const TypeSpecificParamMap& moduleSpecificParameters,
        const QHostAddress& multicastGroupAddress = defaultModuleRevealMulticastGroup,
        const unsigned int multicastGroupPort = defaultModuleRevealMulticastGroupPort );
    virtual ~NetworkOptixModuleRevealer();

public slots:
    virtual void pleaseStop() override;

protected:
    virtual void run() override;

private:
    aio::PollSet m_pollSet;
    std::vector<UDPSocket*> m_sockets;
    RevealResponse m_revealResponse;
};

#endif  //NETWORKOPTIXMODULEREVEALER_H
