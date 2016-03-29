#include <gtest/gtest.h>
#include <nx/network/nettools.h>

#include <QTextStream>

#if 0
// TODO: test on tun/tap?
TEST(CommonNetwork, DISABLED_NetTools)
{
    QHash<QByteArray, QByteArray> netIfs;
    for (const auto& netIf : getAllIPv4Interfaces())
    {
        QByteArray data;
        QTextStream stream(&data, QIODevice::WriteOnly);
        stream << netIf.address.toString() << " " << getGatewayOfIf(netIf.name).toString();

        stream.flush();
        netIfs[netIf.name.toUtf8()] = data;
    }

    EXPECT_EQ(netIfs["eth0"], QByteArray("10.0.2.134 10.0.2.103"));
}
#endif
