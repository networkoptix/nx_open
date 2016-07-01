/**********************************************************
* 30 sep 2013
* a.kolesnikov
***********************************************************/

#include "rns_product_parameters.h"

#include <QtCore/QVariant>


ProductResourcesNameset::ProductResourcesNameset()
{
    registerResource( ProductParameters::product, "product", QVariant::String );
    registerResource( ProductParameters::customization, "customization", QVariant::String );
    registerResource( ProductParameters::module, "module", QVariant::String );
    registerResource( ProductParameters::version, "version", QVariant::String );
    registerResource( ProductParameters::mirrorUrl, "mirrorUrl", QVariant::String );
    registerResource( ProductParameters::arch, "arch", QVariant::String );
    registerResource( ProductParameters::platform, "platform", QVariant::String );
}
