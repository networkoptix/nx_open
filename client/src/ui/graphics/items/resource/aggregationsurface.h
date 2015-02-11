/**********************************************************
* 24 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef AGGREGATIONSURFACE_H
#define AGGREGATIONSURFACE_H

#include <map>
#include <vector>

extern "C"
{
    #include <libavutil/pixfmt.h>
}

#include <utils/common/mutex.h>
#include <QtCore/QRect>
#include <QtGui/QRegion>
#include <QtCore/QScopedPointer>
#include <QSharedPointer>
#include <QtCore/QSize>


class QnGlRendererTexture1;
class GLContext;

/*!
    \note All region-arithmetic is thread-safe, but synchronization of memory operations is out of this class
*/
class AggregationSurface
{
public:
    /*!
        Required opengl textures are created here, so appropriate opengl context MUST be current
        \param format Format is stored data. Supported formats are \a PIX_FMT_YUV420P and \a PIX_FMT_NV12 
        \param size Surface size in pixels
    */
    AggregationSurface( PixelFormat format, const QSize& size );

    //!Ensures that \a requiredRect is uploaded to opengl memory
    /*!
        Can create ogl texture(s), if needed.
        Current ogl context MUST NOT be NULL
        \note Simultaneous call of this method in multiple thread can result in undefined behavour
    */
    void ensureUploadedToOGL( const QRect& requiredRect, qreal opacity = 1.0 );
    const std::vector<GLuint>& glTextures() const;
    //!Copies data from \a planes to \a destRect
    /*!
        Assumes that input data is in surface format and that there is enough input data to fill \a destRect
        \note This method does not check anything or imply any synchronization
    */
    void uploadData( const QRect& destRect, uint8_t* planes[], int lineSizes[] );
    //!Checked is \a rect is not locked and locks it
    /*!
        \return true If \a rect has been locked. false, otherwise
    */
    bool lockRect( const QRect& rect );
    //!Checks if rect of size \a requestedRectSize is available some where and, if found, locks it
    /*!
        \return Locked rect or \a QRect() if no rect of size \a requestedRectSize could be locked
    */
    QRect findAndLockRect( const QSize& requestedRectSize );
    //!Unlocks rect of size \a rect
    void unlockRect( const QRect& rect );
    //!Returns rectangle in gl texture coordinates corresponding to \a rect of system memory surface
    QRectF translateRectToTextureRect( const QRect& rect ) const;

private:
    class SysMemPlane
    {
    public:
        std::basic_string<quint8> buffer;
        size_t pitch;

        SysMemPlane()
        :
            pitch( 0 )
        {
        }
    };

    static const unsigned int MAX_PLANE_COUNT = 3;

    mutable QnMutex m_mutex;
    mutable QnMutex m_uploadMutex;
    mutable std::vector<GLuint> m_glTextureIDs;
    QScopedPointer<QnGlRendererTexture1> m_textures[MAX_PLANE_COUNT];
    SysMemPlane m_buffers[MAX_PLANE_COUNT];
    //!format of data in system memory buffers
    PixelFormat m_format;
    //!format of data in opengl memory
    PixelFormat m_textureFormat;
    bool m_convertToRgb;
    uchar* m_yuv2rgbBuffer;
    int m_yuv2rgbBufferLen;
    //!Locked region of \a m_buffers
    QRegion m_lockedSysMemBufferRegion;
    //!Sub region of \a m_lockedSysMemBufferRegion which has been uploaded to opengl. It is garanteed that data in this region corresponds to that in \a m_buffers
    QRegion m_glMemRegion;
    //!Unlocked rects are added to this region
    QRegion m_invalidatedRegion;
    const QRect m_fullRect;
    size_t m_lockedRectCount;
    size_t m_planeCount;
};

class AggregationSurfacePool;

/*!
    Explicit creation of objects of this class is not recommended. Should use AggregationSurfacePool::takeSurfaceRect
*/
class AggregationSurfaceRect
{
public:
    AggregationSurfaceRect( const QSharedPointer<AggregationSurface>& surface, const QRect& rect ); 
    ~AggregationSurfaceRect();

    const QSharedPointer<AggregationSurface>& surface() const;
    const QRect& rect() const;
    const QRectF& texCoords() const;
    void uploadData(
        uint8_t* planes[],
        int lineSizes[],
        size_t planeCount );
    //!Ensures that held rect is uploaded to opengl texture
    /*!
        This method MUST be called with suitable opengl context current (this may be context passed to \a AggregationSurfacePool::takeSurfaceRect ir )
    */
    void ensureUploadedToOGL( qreal opacity = 1.0 );

private:
    QSharedPointer<AggregationSurface> m_surface;
    QRect m_rect;
    QRectF m_texCoords;
};

/*!
    Pool of surfaces. Each surface is associated with opengl context, which MUST be specified to \a takeSurfaceRect call
*/
class AggregationSurfacePool
{
public:
    //!Finds a surface with required \a format containing unused region of size \a requiredEmptySize and marks this region as used
    /*!
        \param glContext OGL context, with which returned surface manipulations will be done
        \return NULL, if not found
        \note Method assumes that context \a glContext is current
    */
    QSharedPointer<AggregationSurfaceRect> takeSurfaceRect(
        const GLContext* glContext,
        const PixelFormat format,
        const QSize& requiredEmptySize );
    //!Removes all surfaces associated with \a glContext. \a glContext MUST NOT be NULL
    void removeSurfaces( GLContext* const glContext );

    static AggregationSurfacePool* instance();

private:
    typedef std::multimap<std::pair<const GLContext*, PixelFormat>, QSharedPointer<AggregationSurface> > AggregationSurfaceContainer;

    QnMutex m_mutex;
    AggregationSurfaceContainer m_surfaces;
};

#endif  //AGGREGATIONSURFACE_H
