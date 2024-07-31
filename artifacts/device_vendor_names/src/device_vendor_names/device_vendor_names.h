// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QHash>
#include <QtCore/QString>

inline QString shortVendorByName(const QString& vendor)
{
    static const QHash<QString, QString> shortVendorNames =
    {
        // TODO: Consider moving this declaration of vendor short names to resource_data.json.
        {"digital watchdog", "dw"},
        {"digital_watchdog", "dw"},
        {"digitalwatchdog", "dw"},
        {"panoramic", "dw"},
        {"ipnc", "dw"},
        {"acti corporation", "acti"},
        {"innovative security designs", "isd"},
        {"norbain_", "vista"},
        {"norbain", "vista"},
        {"flir systems", "flir"},
        {"hanwha techwin", "hanwha"},
        {"hanwha_techwin", "hanwha"},
        {"hanwha vision", "hanwha"},
        {"samsung techwin", "samsung"},
        {"2n telecommunications", "2nt"},
        {"hangzhou hikvision digital technology co., ltd", "hikvision"},
        {"arecont vision", "arecontvision"},
        {"uniview tec", "uniview"},
        {"uniview_tec", "uniview"},
        {"univiewtec", "uniview"},
        {"unv", "uniview"},
    };
    return shortVendorNames.value(vendor, vendor);
}
