
#pragma once

#include <client/client_color_types.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <ui/graphics/items/generic/proxy_label.h>

class QnBookmarkTagsControl : public QnProxyLabel
{
    Q_OBJECT

    Q_PROPERTY(QnBookmarkColors colors READ colors WRITE setColors)

public:
    QnBookmarkTagsControl(const QnCameraBookmarkTags &tags
        , QGraphicsItem *parent = nullptr);

    virtual ~QnBookmarkTagsControl();

    void setTags(const QnCameraBookmarkTags &tags);

    const QnBookmarkColors &colors() const;

    void setColors(const QnBookmarkColors &colors);

signals:
    void tagClicked(const QString &tag);

private:
    void updateCurrentTag(const QString &currentTag);

    void onLinkHovered(const QString &tag);

    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent * event) override;

private:
    QnBookmarkColors m_colors;
    QnCameraBookmarkTags m_tags;
};