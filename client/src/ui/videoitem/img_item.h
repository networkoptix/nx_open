#ifndef img_item_h_2211
#define img_item_h_2211

#include "abstract_scene_item.h"

// this is abstract class
// I assume that it inherits by video and static_image item
class CLImageItem : public CLAbstractSceneItem
{
    Q_OBJECT
public:
    CLImageItem(GraphicsView *view, int max_width, int max_height, QString name = QString());

    virtual int height() const;
    virtual int width() const;

    virtual float aspectRatio() const;
    QSize imageSize() const;

    bool showImageSize() const;
    void setShowImageSize(bool show);

    QString infoText() const;
    void setInfoText(const QString &text);

    bool showInfoText() const;
    void setShowInfoText(bool show);

    QString extraInfoText() const;
    void setExtraInfoText(const QString &text);

Q_SIGNALS:
    void onAspectRatioChanged(CLAbstractSceneItem *item);

protected:
    virtual void drawStuff(QPainter* painter);
    virtual void drawInfoText(QPainter* painter);

    virtual bool wantText() const;

protected:
    mutable QMutex m_mutex;
    int m_imageWidth;
    int m_imageHeight;

    int m_imageWidth_old;
    int m_imageHeight_old;

    mutable QMutex m_mutex_aspect;
    float m_aspectratio;

    // unprotected
    bool m_showimagesize;
    bool m_showinfotext;

    QString m_infoText;
    QString m_extraInfoText;

    bool m_showing_text; // showing text now
    QTime m_textTime; // draw text is very very very expensive. so want to delay it a bit
    bool m_timeStarted;

    QFont m_Info_Font;
};

#endif //img_item_h_2211
