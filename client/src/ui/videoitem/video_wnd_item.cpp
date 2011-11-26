#include "video_wnd_item.h"
#include "camera/camera.h"
#include "ui/graphicsview.h"
#include "settings.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "utils/media/frame_info.h"


//(videoCodecCtx && videoCodecCtx->sample_aspect_ratio.num) ? av_q2d(videoCodecCtx->sample_aspect_ratio) : 1.0f;

CLVideoWindowItem::CLVideoWindowItem(GraphicsView* view, const QnVideoResourceLayout* layout, int max_width, int max_height, QString name) :
    CLImageItem(view, max_width, max_height, name),
    m_first_draw(true),
    m_showfps(false),
    m_FPS_Font(QLatin1String("Courier New"), 250),
    m_opacity(0.0),
    m_videolayout(layout),
    m_videonum(layout->numberOfChannels())
{

    m_aspectratio = m_aspectratio/layout->width()*layout->height();

    m_FPS_Font.setWeight(QFont::Bold);

    m_zoomOnhover = false;

    m_type = VIDEO;
    m_arranged = false;

    memset(m_stat, 0 , sizeof(m_stat));

    //we never cache the video
    //setCacheMode(DeviceCoordinateCache);
    //setCacheMode(ItemCoordinateCache);
    //setFlag(QGraphicsItem::ItemIsMovable);

    for (unsigned i = 0; i  < m_videonum; ++i)
        m_gldraw[i] = new CLGLRenderer(this);

    connect( this, SIGNAL(onAspectRatioChanged(CLAbstractSceneItem*)), this, SLOT(onResize()));
}

CLVideoWindowItem::~CLVideoWindowItem()
{
    for (unsigned i = 0; i  < m_videonum; ++i)
        delete m_gldraw[i];
}

void CLVideoWindowItem::beforeDestroy()
{
    for (unsigned i = 0; i  < m_videonum; ++i)
        m_gldraw[i]->beforeDestroy();;
}

CLVideoCamera* CLVideoWindowItem::getVideoCam() const
{
    return static_cast<CLVideoCamera*>(m_complicatedItem);
}

QSize CLVideoWindowItem::sizeOnScreen(unsigned int /*channel*/) const
{
    return QSize(onScreenSize().width() / m_videolayout->width(), onScreenSize().height() / m_videolayout->height());
}

bool CLVideoWindowItem::constantDownscaleFactor() const
{
    return (m_view->shouldOptimizeDrawing() && !isFullScreen());
}

bool CLVideoWindowItem::isZoomable() const
{
    return true;
}

void CLVideoWindowItem::goToSteadyMode(bool steady, bool instant)
{
    CLImageItem::goToSteadyMode(steady, instant);
    if (steady)
    {
        scene()->views().at(0)->viewport()->setCursor(Qt::BlankCursor);
    }
    else
    {
        scene()->views().at(0)->viewport()->setCursor(Qt::OpenHandCursor);
    }

}

void CLVideoWindowItem::setItemSelected(bool sel, bool animate, int delay )
{
    CLImageItem::setItemSelected(sel, animate, delay);

    if (m_selected)
    {
        if (dynamic_cast<QnAbstractArchiveReader*>(getVideoCam()->getStreamreader()))
        {
            getVideoCam()->getCamCamDisplay()->setMTDecoding(true);
        }

        getVideoCam()->setQuality(QnQualityHighest, true);
        getVideoCam()->getCamCamDisplay()->playAudio(true);
    }
    else
    {
        getVideoCam()->getCamCamDisplay()->setMTDecoding(false);
        getVideoCam()->setQuality(QnQualityNormal, false);
        getVideoCam()->getCamCamDisplay()->playAudio(false);
    }

}

void CLVideoWindowItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    CLImageItem::hoverEnterEvent(event);

    QnResourcePtr dev = getComplicatedItem()->getDevice();
    if (!dev->checkFlag(QnResource::SINGLE_SHOT))
    {
        setShowImageSize(true);
    }

    //if (!dev->checkFlag(CLDevice::ARCHIVE) && !dev->checkFlag(CLDevice::SINGLE_SHOT) )
    if (!dev->checkFlag(QnResource::SINGLE_SHOT) )
    {
        showFPS(true);
        setShowInfoText(true);
    }
}

void CLVideoWindowItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    CLImageItem::hoverLeaveEvent(event);

    if (m_view->getSelectedItem()!=this)
    {
        setShowImageSize(false);
        showFPS(false);
        setShowInfoText(false);
    }
}

void CLVideoWindowItem::waitForFrameDisplayed(int channel)
{
    m_gldraw[channel]->waitForFrameDisplayed(channel);
}


void CLVideoWindowItem::draw(CLVideoDecoderOutput* image)
{
    // this is not ui thread
    m_first_draw = false;

    m_gldraw[image->channel]->draw(image); // this function will wait m_gldraw.paintEvent(0);

    //needUpdate(true);
    QMutexLocker locker(&m_mutex);
    m_imageWidth = image->width * image->sample_aspect_ratio;
    m_imageHeight = image->height;

    if (m_imageWidth!=m_imageWidth_old || m_imageHeight!=m_imageHeight_old)
    {
        {
            QMutexLocker  locker(&m_mutex_aspect);
            m_aspectratio = qreal(m_imageWidth)/m_imageHeight;

            m_imageWidth_old = m_imageWidth;
            m_imageHeight_old = m_imageHeight;
        }

        emit onAspectRatioChanged(this);
    }
}


QPointF CLVideoWindowItem::getBestSubItemPos(CLSubItemType type)
{
    switch(type)
    {
    case CloseSubItem:
        return QPointF(width()-310, 20);

    case MakeScreenshotSubItem:
        return QPointF(width()-310, 320);

    case RecordingSubItem:
        return QPointF(-30, 100);

    default:
        break;
    }

    return QPointF(-1001, -1001);
}

QRect CLVideoWindowItem::getSubChannelRect(unsigned int channel) const
{
    if (m_videonum==1) // most case scenario
        return QRect(0,0,width(),height());

    /*

    *----*----*
    |    |    |
    |    |    |
    *----*----*
    |    |    |
    |    |    |
    *----*----*

    */

    int cel_x = m_videolayout->h_position(channel);
    int cel_y = m_videolayout->v_position(channel);

    int w = width()/m_videolayout->width();
    int h = height()/m_videolayout->height();

    return QRect(cel_x*w, cel_y*h, w,h);
}

void CLVideoWindowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* /*widget*/)
{
    if (m_first_draw)
        return;

    if (painter->paintEngine()->type() != QPaintEngine::OpenGL && painter->paintEngine()->type() != QPaintEngine::OpenGL2)
        return;

    //painter->setClipping(true);
    //painter->setClipRect( option->exposedRect );

    // save the GL state set for QPainter
    if (m_steadyMode)
        drawSteadyWall(painter);

    bool drawSelection = option->state & QStyle::State_Selected;

    painter->beginNativePainting();
    saveGLState();

    for (unsigned i = 0; i  < m_videonum; ++i)
    {
        m_gldraw[i]->setOpacity(drawSelection ? 0.7 : 1.0);
        if (m_gldraw[i]->paintEvent(getSubChannelRect(i)) == CLGLRenderer::CANNOT_RENDER) 
        {
            drawGLfailaure(painter);
        } 
        else 
        {
            QString timeText = m_gldraw[i]->getTimeText();
            if (!timeText.isEmpty())
            {
                QFont font;
                font.setPixelSize(128);
                QFontMetrics metric(font);
                QSize size = metric.size(Qt::TextSingleLine, timeText);
                painter->setFont(font);
                painter->drawText(width() - size.width() - 8, height() - size.height() - 8, timeText);
            }
            if (m_gldraw[i]->isNoVideo())
            {
                QString text = "No video";
                QFont font;
                font.setPixelSize(192);
                //QFontMetrics metric(font);
                //QSize size = metric.size(Qt::TextSingleLine, text);
                painter->setFont(font);
                QPen pen;
                pen.setColor(QColor(255,0,0));
                painter->setPen(pen);
                painter->drawText( QRect(0,0,width(),height()), Qt::AlignHCenter | Qt::AlignCenter, text);
            }
        }
    }

    // restore the GL state that QPainter expects

    restoreGLState();

    painter->endNativePainting();
    drawStuff(painter);

    if (drawSelection)
    {
        painter->fillRect(QRect(0, 0, width(), height()),
            m_can_be_droped ? global_can_be_droped_color :  global_selection_color);
    }

    frameDisplayed();
}

bool CLVideoWindowItem::wantText() const
{
    return (CLImageItem::wantText() || m_showfps);
}

void CLVideoWindowItem::drawStuff(QPainter* painter)
{
    if (m_steadyMode)
        return;

    CLImageItem::drawStuff(painter);

    drawShadow(painter);

    //============
    if (m_showing_text && m_showfps && global_show_item_text)
    {
        drawFPS(painter);	// ahtung! drawText takes huge(!) ammount of cpu
    }

    if (m_stat[0] && m_stat[0]->isConnectioLost())
        drawLostConnection(painter);

}
void CLVideoWindowItem::drawFPS(QPainter* painter)
{
    painter->setFont(m_FPS_Font);

    //painter->setPen(QColor(0,255,0,170));
    painter->setPen(QColor(0,240,240,170));

    char fps[100];

    for (unsigned i = 0; i < m_videonum; ++i)
    {
        if (m_stat[i]==0)
            continue;

        QRect rect = getSubChannelRect(i);

#ifdef Q_CC_MSVC
        sprintf_s(fps, "%6.2ffps %6.2fMbps", m_stat[i]->getFrameRate(), m_stat[i]->getBitrate());
#else
        sprintf(fps, "%6.2ffps %6.2fMbps", m_stat[i]->getFrameRate(), m_stat[i]->getBitrate());
#endif
        QFontMetrics fm(painter->font());
        painter->drawText(rect.left()+150,rect.top()+170 + fm.height()/2, QString::fromLatin1(fps));
    }
}

void CLVideoWindowItem::drawGLfailaure(QPainter* painter)
{
    painter->setFont(m_FPS_Font);

    QString text;
    QTextStream(&text) << tr("Image size is bigger than MAXGlTextureSize(") << CLGLRenderer::getMaxTextureSize() << ") on this video hardware. Such images cannot be displayed in this version." ;

    QFontMetrics metrics = QFontMetrics(m_FPS_Font);
    int border = qMax(4, metrics.leading());

    QRect rect(0, 0, width() , height());

    painter->fillRect(QRect(0, 0, width(), height()),
        QColor(255, 0, 0, 85));

    painter->setPen(QColor(255, 0, 0, 110));

    painter->drawText((width() - rect.width())/2, border,
        rect.width(), rect.height(),
        Qt::AlignCenter | Qt::TextWordWrap, text);
}

void CLVideoWindowItem::drawLostConnection(QPainter* painter)
{
    painter->setFont(m_FPS_Font);

    QString text = tr("Connection Lost");

    QFontMetrics metrics = QFontMetrics(m_FPS_Font);
    int border = qMax(4, metrics.leading());

    QRect rect(0, 0, width() , height());

    painter->fillRect(QRect(0, 0, width(), height()),
        QColor(255, 0, 0, 85));

    painter->setPen(QColor(255, 0, 0, 110));

    painter->drawText((width() - rect.width())/2, border,
        rect.width(), rect.height(),
        Qt::AlignCenter | Qt::TextWordWrap, text);
}

void CLVideoWindowItem::saveGLState()
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    //glMatrixMode(GL_PROJECTION);
    //glPushMatrix();
    //glMatrixMode(GL_MODELVIEW);
    //glPushMatrix();
}

void CLVideoWindowItem::restoreGLState()
{
    //glMatrixMode(GL_PROJECTION);
    //glPopMatrix();
    //glMatrixMode(GL_MODELVIEW);
    //glPopMatrix();
    glPopAttrib();
}

void CLVideoWindowItem::showFPS(bool show)
{
    m_showfps = show;
}

void CLVideoWindowItem::setStatistics(QnStatistics * st, unsigned int channel)
{
    m_stat[channel] = st;
}

float CLVideoWindowItem::aspectRatio() const
{
    QMutexLocker locker(&m_mutex_aspect);

    if (m_videonum == 1) // most case scenario; for optimization
        return m_aspectratio;

    //=============================
    //m_videonum >1
    // at this point we assume that all channels have the same aspect ratio;
    return m_aspectratio*m_videolayout->width()/m_videolayout->height();
}

QImage CLVideoWindowItem::getScreenshot()
{
    if (getVideoLayout()->numberOfChannels() == 0)
        return QImage();
    unsigned width = 0;
    unsigned height = 0;
    QList<QImage> screens;
    for (unsigned i = 0; i < getVideoLayout()->numberOfChannels(); ++i)
    {
        screens << getVideoCam()->getCamCamDisplay()->getScreenshot(i);
        width = qMax(width, (getVideoLayout()->h_position(i)+1)*screens[0].width());
        height = qMax(height, (getVideoLayout()->v_position(i)+1)*screens[0].height());
    }
    QImage rez(width, height, QImage::Format_ARGB32);
    QPainter p(&rez);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    for (unsigned i = 0; i < getVideoLayout()->numberOfChannels(); ++i)
        p.drawImage(QPoint(getVideoLayout()->h_position(i)*screens[0].width(), getVideoLayout()->v_position(i)*screens[0].height()), screens[i]);
    p.end();
    return rez;
}

bool CLVideoWindowItem::contains(CLAbstractRenderer* renderer)  const
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        if (m_gldraw[i] == renderer)
            return true;
    }
    return false;
}
