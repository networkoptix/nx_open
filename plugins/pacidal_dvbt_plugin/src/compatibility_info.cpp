#include "compatibility.h"

QList<QnCompatibilityItem> localCompatibilityItems()
{
    QList<QnCompatibilityItem> items;

    items.append(QnCompatibilityItem(QLatin1String("2.0"), QLatin1String("android"), QLatin1String("2.1")));
    items.append(QnCompatibilityItem(QLatin1String("2.1"), QLatin1String("android"), QLatin1String("2.0")));
    items.append(QnCompatibilityItem(QLatin1String("2.1"), QLatin1String("android"), QLatin1String("1.6")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("android"), QLatin1String("2.0")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("android"), QLatin1String("2.1")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("android"), QLatin1String("1.4")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("android"), QLatin1String("1.5")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("android"), QLatin1String("1.6")));
    items.append(QnCompatibilityItem(QLatin1String("2.0"), QLatin1String("MediaServer"), QLatin1String("2.1")));
    items.append(QnCompatibilityItem(QLatin1String("2.0"), QLatin1String("MediaServer"), QLatin1String("1.6")));
    items.append(QnCompatibilityItem(QLatin1String("2.1"), QLatin1String("MediaServer"), QLatin1String("2.0")));
    items.append(QnCompatibilityItem(QLatin1String("2.1"), QLatin1String("MediaServer"), QLatin1String("1.6")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("MediaServer"), QLatin1String("2.0")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("MediaServer"), QLatin1String("2.1")));
    items.append(QnCompatibilityItem(QLatin1String("2.0"), QLatin1String("iOSClient"), QLatin1String("2.0")));
    items.append(QnCompatibilityItem(QLatin1String("2.0"), QLatin1String("iOSClient"), QLatin1String("2.1")));
    items.append(QnCompatibilityItem(QLatin1String("2.0"), QLatin1String("iOSClient"), QLatin1String("2.2")));
    items.append(QnCompatibilityItem(QLatin1String("2.0"), QLatin1String("iOSClient"), QLatin1String("1.5")));
    items.append(QnCompatibilityItem(QLatin1String("2.0"), QLatin1String("iOSClient"), QLatin1String("1.6")));
    items.append(QnCompatibilityItem(QLatin1String("2.1"), QLatin1String("iOSClient"), QLatin1String("2.0")));
    items.append(QnCompatibilityItem(QLatin1String("2.1"), QLatin1String("iOSClient"), QLatin1String("2.1")));
    items.append(QnCompatibilityItem(QLatin1String("2.1"), QLatin1String("iOSClient"), QLatin1String("2.2")));
    items.append(QnCompatibilityItem(QLatin1String("2.1"), QLatin1String("iOSClient"), QLatin1String("1.5")));
    items.append(QnCompatibilityItem(QLatin1String("2.1"), QLatin1String("iOSClient"), QLatin1String("1.6")));
    items.append(QnCompatibilityItem(QLatin1String("2.2"), QLatin1String("iOSClient"), QLatin1String("2.0")));
    items.append(QnCompatibilityItem(QLatin1String("2.2"), QLatin1String("iOSClient"), QLatin1String("2.1")));
    items.append(QnCompatibilityItem(QLatin1String("2.2"), QLatin1String("iOSClient"), QLatin1String("2.2")));
    items.append(QnCompatibilityItem(QLatin1String("2.2"), QLatin1String("iOSClient"), QLatin1String("1.5")));
    items.append(QnCompatibilityItem(QLatin1String("2.2"), QLatin1String("iOSClient"), QLatin1String("1.6")));
    items.append(QnCompatibilityItem(QLatin1String("2.3"), QLatin1String("iOSClient"), QLatin1String("2.0")));
    items.append(QnCompatibilityItem(QLatin1String("2.3"), QLatin1String("iOSClient"), QLatin1String("2.1")));
    items.append(QnCompatibilityItem(QLatin1String("2.3"), QLatin1String("iOSClient"), QLatin1String("2.2")));
    items.append(QnCompatibilityItem(QLatin1String("2.3"), QLatin1String("iOSClient"), QLatin1String("1.5")));
    items.append(QnCompatibilityItem(QLatin1String("2.3"), QLatin1String("iOSClient"), QLatin1String("1.6")));
    items.append(QnCompatibilityItem(QLatin1String("1.5"), QLatin1String("iOSClient"), QLatin1String("2.0")));
    items.append(QnCompatibilityItem(QLatin1String("1.5"), QLatin1String("iOSClient"), QLatin1String("2.1")));
    items.append(QnCompatibilityItem(QLatin1String("1.5"), QLatin1String("iOSClient"), QLatin1String("2.2")));
    items.append(QnCompatibilityItem(QLatin1String("1.5"), QLatin1String("iOSClient"), QLatin1String("1.5")));
    items.append(QnCompatibilityItem(QLatin1String("1.5"), QLatin1String("iOSClient"), QLatin1String("1.6")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("iOSClient"), QLatin1String("2.0")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("iOSClient"), QLatin1String("2.1")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("iOSClient"), QLatin1String("2.2")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("iOSClient"), QLatin1String("1.5")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("iOSClient"), QLatin1String("1.6")));
    items.append(QnCompatibilityItem(QLatin1String("2.0"), QLatin1String("Client"), QLatin1String("1.6")));
    items.append(QnCompatibilityItem(QLatin1String("2.1"), QLatin1String("Client"), QLatin1String("2.0")));
    items.append(QnCompatibilityItem(QLatin1String("2.1"), QLatin1String("Client"), QLatin1String("1.6")));
    items.append(QnCompatibilityItem(QLatin1String("1.6"), QLatin1String("Client"), QLatin1String("2.0")));

    return items;
}
