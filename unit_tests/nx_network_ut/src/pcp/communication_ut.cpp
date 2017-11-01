#include <gtest/gtest.h>

#include <nx/network/pcp/sender.h>
#include <nx/network/pcp/listener.h>

#include "data_stream_helpers.h"

using namespace pcp;

class TestEvents : public Listener::Events
{
public:
    virtual void handle(const ResponseHeadeer&)
    {
        qDebug() << "Announce";
    }

    virtual void handle(const ResponseHeadeer&, const MapMessage&)
    {
        qDebug() << "Map";
    }

    virtual void handle(const ResponseHeadeer&, const PeerMessage&)
    {
        qDebug() << "Peer";
    }
};

TEST(PcpCommunication, Simple)
{
    TestEvents events;
    Listener listener(events);
    Sender sender("10.0.2.1");

    HostAddress host("10.0.2.64");
    const RequestHeader head = { VERSION, Opcode::MAP, 60*60, dsh::rawBytes(*host.ipV6().first) };
    const MapMessage msg = { makeRandomNonce(), 0, 80, 8877, QByteArray(16, 0) };

    sender.send(head, msg);
    QThread::sleep(1);
}
