// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsWidget>

#include <nx/utils/impl_ptr.h>

struct PlaybackPositionIconTextWidgetOptions
{
    static constexpr int kDefaultItemWidth = 250;
    static constexpr int kDefaultPadding = 8;
    static constexpr int kDefaultSpacing = 5;

    QColor backgroundColor;
    int maxWidth = kDefaultItemWidth;
    int borderRadius = 0;
    int horPadding = kDefaultPadding;
    int vertPadding = kDefaultPadding;
    int horSpacing = kDefaultSpacing;
    int fixedHeight = 0;

    bool operator==(const PlaybackPositionIconTextWidgetOptions& other) const = default;
};

/**
 * Graphics widget that is used on bottom overlay to store icon and/or description.
 */
class PlaybackPositionIconTextWidget: public QGraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

    typedef QGraphicsWidget base_type;

public:
    PlaybackPositionIconTextWidget(
        const QString& text = QString(),
        const PlaybackPositionIconTextWidgetOptions& options = PlaybackPositionIconTextWidgetOptions(),
        QGraphicsItem* parent = nullptr);

    ~PlaybackPositionIconTextWidget();

    QString text() const;
    void setText(const QString& html);

    void setIcon(const QPixmap& icon);

    PlaybackPositionIconTextWidgetOptions options() const;
    void setOptions(const PlaybackPositionIconTextWidgetOptions& options);

private:
    virtual void paint(
        QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
