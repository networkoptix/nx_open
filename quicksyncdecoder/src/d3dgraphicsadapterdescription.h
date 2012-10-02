////////////////////////////////////////////////////////////
// 1 oct 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef INTELGRAPHICSADAPTERDESCRIPTION_H
#define INTELGRAPHICSADAPTERDESCRIPTION_H

#include <D3D9.h>

#include <QString>

#include <plugins/videodecoders/stree/resourcecontainer.h>


/*!
    Provides resources:\n
    - gpuDeviceString
    - gpuName
    - sdkVersion
*/
class D3DGraphicsAdapterDescription
:
    public stree::AbstractResourceReader
{
public:
    /*!
        \param graphicsAdapterNumber Number of graphics adapter to return info about
    */
    D3DGraphicsAdapterDescription( IDirect3D9Ex* direct3D9, unsigned int graphicsAdapterNumber );

    //!Implementation of AbstractResourceReader::get
    virtual bool get( int resID, QVariant* const value ) const;

private:
    int m_graphicsAdapterNumber;
    QString m_driverVersionStr;
    D3DADAPTER_IDENTIFIER9 m_adapterIdentifier;
};

#endif  //INTELGRAPHICSADAPTERDESCRIPTION_H
