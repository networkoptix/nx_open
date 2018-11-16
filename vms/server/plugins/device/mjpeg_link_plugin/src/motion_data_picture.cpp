#include "motion_data_picture.h"

#include <climits>
#include <cstring>

#include <nx/kit/utils.h>

namespace nx::vms_server_plugins::mjpeg_link {

MotionDataPicture::MotionDataPicture()
:
    m_refManager( this ),
    m_data( NULL ),
    m_width( nxcip::DEFAULT_MOTION_DATA_PICTURE_HEIGHT ),
    m_height( nxcip::DEFAULT_MOTION_DATA_PICTURE_WIDTH ),
    m_stride( nx::kit::utils::alignUp( m_width, CHAR_BIT ) / CHAR_BIT )
{
    m_data = static_cast<uint8_t*>(nx::kit::utils::mallocAligned( m_stride*m_height, nxcip::MEDIA_DATA_BUFFER_ALIGNMENT ));
    memset( m_data, 0, m_stride*m_height );
}

MotionDataPicture::~MotionDataPicture()
{
    nx::kit::utils::freeAligned( m_data );
    m_data = NULL;
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* MotionDataPicture::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_Picture, sizeof(nxcip::IID_Picture) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::Picture*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

//!Implementation of nxpl::PluginInterface::addRef
unsigned int MotionDataPicture::addRef()
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
unsigned int MotionDataPicture::releaseRef()
{
    return m_refManager.releaseRef();
}

//!Returns pixel format
nxcip::PixelFormat MotionDataPicture::pixelFormat() const
{
    return nxcip::AV_PIX_FMT_MONOBLACK;
}

int MotionDataPicture::planeCount() const
{
    return 1;
}

//!Width (pixels)
int MotionDataPicture::width() const
{
    return m_width;
}

//!Height (pixels)
int MotionDataPicture::height() const
{
    return m_height;
}

//!Length of horizontal line in bytes
int MotionDataPicture::xStride( int /*planeNumber*/ ) const
{
    return m_stride;
}

//!Returns pointer to horizontal line \a lineNumber (starting with 0)
const void* MotionDataPicture::scanLine( int /*planeNumber*/, int lineNumber ) const
{
    return m_data + m_stride*lineNumber;
}

void* MotionDataPicture::scanLine( int /*planeNumber*/, int lineNumber )
{
    return m_data + m_stride*lineNumber;
}

/*!
    \return MotionDataPicture data. Returned buffer MUST be aligned on \a MEDIA_DATA_BUFFER_ALIGNMENT - byte boundary (this restriction helps for some optimization).
        \a nx::kit::utils::mallocAligned and \a nx::kit::utils::freeAligned routines can be used for that purpose
*/
const void* MotionDataPicture::data() const
{
    return m_data;
}

void* MotionDataPicture::data()
{
    return m_data;
}

void MotionDataPicture::setPixel( int x, int y, int val )
{
    const size_t bitPos = x % CHAR_BIT;
    uint8_t* thatVeryByte = (m_data + m_stride*y) + (x / CHAR_BIT);
    if( val )
        *thatVeryByte |= uint8_t(1 << bitPos);
    else
        *thatVeryByte &= ~(uint8_t(1 << bitPos));
}

void MotionDataPicture::fillRect( int left, int top, int width, int height, int val )
{
    for( int y = top; y < top + height; ++y )
    {
        for( int x = left; x < left + width; ++x )
            setPixel( x, y, val );
    }
}

} // namespace nx::vms_server_plugins::mjpeg_link
