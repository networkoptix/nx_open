/**********************************************************
* 12 sep 2013
* akolesnikov
***********************************************************/

#ifndef ILP_MOTION_DATA_PICTURE_H
#define ILP_MOTION_DATA_PICTURE_H

#include <stdint.h>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>


/*!
    Supports only \a nxcip::PIX_FMT_MONOBLACK and \a nxcip::PIX_FMT_GRAY8
*/
class MotionDataPicture
:
    public nxcip::Picture
{
public:
    /*!
        Supports only \a nxcip::PIX_FMT_MONOBLACK and \a nxcip::PIX_FMT_GRAY8
    */
    MotionDataPicture( nxcip::PixelFormat _pixelFormat );
    virtual ~MotionDataPicture();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    //!Returns pixel format
    virtual nxcip::PixelFormat pixelFormat() const override;
    virtual int planeCount() const override;
    //!Width (pixels)
    virtual int width() const override;
    //!Hidth (pixels)
    virtual int height() const override;
    //!Length of horizontal line in bytes
    virtual int xStride( int planeNumber ) const override;
    //!Returns pointer to horizontal line \a lineNumber (starting with 0)
    virtual const void* scanLine( int planeNumber, int lineNumber ) const override;
    virtual void* scanLine( int planeNumber, int lineNumber ) override;
    /*!
        \return Picture data. Returned buffer MUST be aligned on \a MEDIA_DATA_BUFFER_ALIGNMENT - byte boundary (this restriction helps for some optimization).
            \a nxpt::mallocAligned and \a nxpt::freeAligned routines can be used for that purpose
    */
    virtual void* data() override;
    virtual const void* data() const override;

    void setPixel( int x, int y, int val );
    void fillRect( int x, int y, int width, int height, int val );

private:
    nxcip::PixelFormat m_pixelFormat;
    nxpt::CommonRefManager m_refManager;
    uint8_t* m_data;
    size_t m_width;
    size_t m_height;
    size_t m_stride;
};

#endif  //ILP_MOTION_DATA_PICTURE_H
