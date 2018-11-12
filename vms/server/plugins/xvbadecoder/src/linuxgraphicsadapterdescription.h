////////////////////////////////////////////////////////////
// 1 oct 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef LINUXGRAPHICSADAPTERDESCRIPTION_H
#define LINUXGRAPHICSADAPTERDESCRIPTION_H

#include <QString>

#include <plugins/videodecoders/stree/resourcecontainer.h>


/*!
    Provides resources:\n
    - architecture
    - gpuDeviceString
    - driverVersion
*/
class LinuxGraphicsAdapterDescription
:
    public nx::utils::stree::AbstractResourceReader
{
public:
    /*!
        \param graphicsAdapterNumber Number of graphics adapter to return info about
    */
    LinuxGraphicsAdapterDescription( unsigned int graphicsAdapterNumber );

    //!Implementation of AbstractResourceReader::get
    virtual bool get( int resID, QVariant* const value ) const;

private:
    int m_graphicsAdapterNumber;
    QString m_driverVersionStr;
    QString m_gpuDeviceString;
};

#endif  //INTELGRAPHICSADAPTERDESCRIPTION_H
