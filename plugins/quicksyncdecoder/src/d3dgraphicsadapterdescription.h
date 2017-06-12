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
    - architecture
    - gpuDeviceString
    - driverVersion
    - gpuVendorId
    - gpuDeviceId
    - gpuRevision
    - graphicAdapterCount
    - displayAdapterDeviceString
*/
class D3DGraphicsAdapterDescription
:
    public nx::utils::stree::AbstractResourceReader
{
public:
    /*!
        \param graphicsAdapterNumber Number of graphics adapter to return info about
    */
    D3DGraphicsAdapterDescription( IDirect3D9Ex* direct3D9, unsigned int graphicsAdapterNumber );

    //!Implementation of AbstractResourceReader::get
    virtual bool get( int resID, QVariant* const value ) const;

private:
    IDirect3D9Ex* m_direct3D9;
    int m_graphicsAdapterNumber;
    QString m_driverVersionStr;
    D3DADAPTER_IDENTIFIER9 m_adapterIdentifier;
    QString m_displayAdapterDeviceString;
};

#endif  //INTELGRAPHICSADAPTERDESCRIPTION_H
