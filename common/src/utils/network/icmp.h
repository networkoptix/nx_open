#ifndef ICMP_H
#define ICMP_H

#include <QtCore/QString>

/** Class should work only on MacOS. Wasn't even run on other OS'es. */
class Icmp
{
public:
    /**
     * @brief ping          Single sync ping to target host address.
     * @param hostAddr      Latin string containing an IPv4 dotted-decimal address.
     * @param timeoutMSec   Reply waiting timeout.
     * @return              True if reply was received correctly, false otherwise.
     */
    static bool ping(const QString& hostAddr, int timeoutMSec);
};

#endif // ICMP_H
