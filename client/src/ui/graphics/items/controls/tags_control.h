
#pragma once

#include <client/client_color_types.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <ui/graphics/items/generic/proxy_label.h>

class QnTagsControl : public QnProxyLabel
{
    Q_OBJECT

    Q_PROPERTY(QnBookmarkColors colors READ colors WRITE setColors NOTIFY colorsChanged)

public:
    QnTagsControl(const QnCameraBookmarkTags &tags
        , QGraphicsItem *parent = nullptr);

    virtual ~QnTagsControl();

    void setTags(const QnCameraBookmarkTags &tags);

    const QnBookmarkColors &colors() const;

    void setColors(const QnBookmarkColors &colors);

signals:
    void tagClicked(const QString &tag);

    void colorsChanged();

private:
    void updateCurrentTag(const QString &currentTag);

    void onLinkHovered(const QString &tag);

    void hoverEnterEvent(QGraphicsSceneHoverEvent * event);

    void hoverLeaveEvent(QGraphicsSceneHoverEvent * event);

private:
    QnBookmarkColors m_colors;
    QnCameraBookmarkTags m_tags;
};