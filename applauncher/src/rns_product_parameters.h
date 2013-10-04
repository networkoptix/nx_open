/**********************************************************
* 30 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef RNS_PRODUCT_PARAMETERS_H
#define RNS_PRODUCT_PARAMETERS_H

#include <plugins/videodecoders/stree/resourcenameset.h>


namespace ProductParameters
{
    enum Value
    {
        //!String
        product = 1,
        //!String
        customization,
        //!String
        module,
        //!String
        version,
        //!String
        mirrorUrl
    };

}

class ProductResourcesNameset
:
    public stree::ResourceNameSet
{
public:
    ProductResourcesNameset();
};

#endif  //RNS_PRODUCT_PARAMETERS_H
