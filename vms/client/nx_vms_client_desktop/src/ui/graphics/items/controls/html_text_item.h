// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsWidget>

class QnHtmlTextItemPrivate;

struct QnHtmlTextItemOptions
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
    bool autosize = false;

    QnHtmlTextItemOptions() = default;

    QnHtmlTextItemOptions(
        const QColor& initBackgroundColor,
        bool initAutosize,
        int initBorderRadius,
        int initHorPadding,
        int initVertPadding,
        int initMaxWidth);

    bool operator==(const QnHtmlTextItemOptions& other) const = default;
};

class QnHtmlTextItem : public QGraphicsWidget {
    Q_OBJECT

    typedef QGraphicsWidget base_type;
public:
    QnHtmlTextItem(const QString &html = QString()
        , const QnHtmlTextItemOptions &options = QnHtmlTextItemOptions()
        , QGraphicsItem *parent = nullptr);

    ~QnHtmlTextItem();

    Q_PROPERTY(QString html READ html WRITE setHtml)
    QString html() const;
    void setHtml(const QString &html);

    void setIcon(const QPixmap& icon);

    QnHtmlTextItemOptions options() const;
    void setOptions(const QnHtmlTextItemOptions &options);

private:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    Q_DECLARE_PRIVATE(QnHtmlTextItem)
    QScopedPointer<QnHtmlTextItemPrivate> d_ptr;
};
