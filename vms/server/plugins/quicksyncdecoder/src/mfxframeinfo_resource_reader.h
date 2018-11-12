////////////////////////////////////////////////////////////
// 4 feb 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef MFXFRAMEINFO_RESOURCE_READER_H
#define MFXFRAMEINFO_RESOURCE_READER_H

#include <mfx/mfxstructures.h>
#include <plugins/videodecoders/stree/resourcecontainer.h>


class MFXFrameInfoResourceReader
:
    public nx::utils::stree::AbstractResourceReader
{
public:
    MFXFrameInfoResourceReader( const mfxFrameInfo& frameInfo );

    //!Implementation of nx::utils::stree::AbstractResourceReader::get
    /*!
        Following parameters are supported:\n
            - framePictureWidth
            - framePictureHeight
            - framePictureSize
            - fps
            - pixelsPerSecond
    */
    virtual bool get( int resID, QVariant* const value ) const override;

private:
    const mfxFrameInfo& m_frameInfo;
};

#endif  //MFXFRAMEINFO_RESOURCE_READER_H
