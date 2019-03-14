
#pragma once

#include <QString>

namespace rtu
{
    namespace helpers
    {
        bool isValidSubnetMask(const QString &mask);

        bool isDiscoverableFromCurrentNetwork(const QString &ip
            , const QString &mask = QString());

        bool isDiscoverableFromNetwork(const QString &ip
            , const QString &mask
            , const QString &subnet
            , const QString &subnetMask);
    }
}