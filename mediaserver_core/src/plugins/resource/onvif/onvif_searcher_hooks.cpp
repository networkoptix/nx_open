#include "onvif_searcher_hooks.h"
#if defined(ENABLE_ONVIF)

namespace nx {
namespace plugins {
namespace onvif {
namespace searcher_hooks {

void commonHooks(EndpointAdditionalInfo* outInfo)
{
    QString lowerName = outInfo->name.toLower();
    auto& name = outInfo->name;
    auto& manufacturer = outInfo->manufacturer;
    auto& location = outInfo->location;

    if (lowerName == lit("nexcom_camera"))
    {
        name.clear();
        manufacturer = lit("Nexcom");
    }
    else if (location.toLower() == lit("canon") && lowerName == lit("camera"))
    {
        name = manufacturer;
        manufacturer = location;
    }
    else if (lowerName == lit("digital watchdog"))
    {
        qSwap(name, manufacturer);
    }
    else if (manufacturer.toLower().startsWith(lit("dwc-")))
    {
        name = manufacturer;
        manufacturer = lit("Digital Watchdog");
    }
    else if (lowerName == lit("sony"))
    {
        qSwap(name, manufacturer);
    }
    else if (lowerName.startsWith(lit("isd ")))
    {
        manufacturer = lit("ISD");
        name = name.mid(4);
    }
    else if (name == lit("ISD"))
    {
        qSwap(name, manufacturer);
    }
    else if (lowerName == lit("networkcamera") && manufacturer.isEmpty())
    {
        name.clear(); // some DW cameras report invalid model in multicast and empty vendor
    }
    else if (lowerName == lit("networkcamera") && manufacturer.toLower().startsWith(lit("dcs-")))
    {
        name = manufacturer;
        manufacturer = lit("DLink");
    }
    else if (lowerName == lit("networkcamera") && manufacturer.toLower().startsWith(lit("sd8363")))
    {
        name = manufacturer;
        manufacturer = lit("VIVOTEK");
    }
    else if ((lowerName.startsWith(lit("vista_")) || lowerName.startsWith(lit("norbain_"))) && manufacturer.toLower().startsWith(lit("vk2-")))
    {
        name = manufacturer;
        manufacturer = lit("VISTA");
    }
    else if (lowerName.startsWith(lit("axis ")))
    {
        manufacturer = lit("AXIS");
    }
    else if (lowerName == lit("dahua"))
    {
        qSwap(name, manufacturer);
    }
    else if (lowerName == lit("vivo_ironman"))
    {
        qSwap(name, manufacturer);
    }
    else if (lowerName == lit("sentry"))
    {
        qSwap(name, manufacturer);
    }
    else if (lowerName == lit("vivotek") && manufacturer.toLower().startsWith(lit("sd")))
    {
        qSwap(name, manufacturer);
    }
    else if (lowerName.startsWith(lit("acti")))
    {
        name = manufacturer;
        manufacturer = lit("ACTi");
    }
    else if (lowerName.startsWith(lit("network optix")))
    {
        name = manufacturer;
        manufacturer = lit("Network Optix");
    }
}

void hikvisionManufacturerReplacement(EndpointAdditionalInfo* outInfo)
{
    const auto kHikvisionManufacturer = lit("Hikvision");
    auto currentManufacturer = outInfo->manufacturer.toLower();
    auto currentName = outInfo->name.toLower();

    bool applyHikvisonManufacturerHook =
        currentName.startsWith(kHikvisionManufacturer.toLower())
        && currentName.endsWith(currentManufacturer);

    if (applyHikvisonManufacturerHook)
    {
        outInfo->manufacturer = kHikvisionManufacturer;
        auto split = outInfo->name.split(L' ');
        if (split.size() == 2)
            outInfo->name = split[1];
        else
            outInfo->name = outInfo->name.mid(kHikvisionManufacturer.size());
    }
}


} // namespace searcher_hooks
} // namespace onvif
} // namespace plugins
} // namespace nx

#endif // defined(ENABLE_ONVIF)
