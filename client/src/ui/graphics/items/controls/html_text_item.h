#pragma once

#include <QtWidgets/QGraphicsWidget>

class QnHtmlTextItemPrivate;

struct QnHtmlTextItemOptions
{
    QColor backgroundColor;
    int maxWidth;
    int borderRadius;
    int horPadding;
    int vertPadding;
    bool autosize;

    QnHtmlTextItemOptions();

    QnHtmlTextItemOptions(const QColor &initBackgroundColor
        , bool initAutosize
        , int initBorderRadius
        , int initHorPadding
        , int initVertPadding
        , int initMaxWidth);
};

class QnHtmlTextItem : public QGraphicsWidget {
    Q_OBJECT

    typedef QGraphicsWidget base_type;
public:
    QnHtmlTextItem(const QString &html
        , const QnHtmlTextItemOptions &options = QnHtmlTextItemOptions()
        , QGraphicsItem *parent = nullptr);

    ~QnHtmlTextItem();

    void setHtml(const QString &html);

private:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    Q_DECLARE_PRIVATE(QnHtmlTextItem)
    QScopedPointer<QnHtmlTextItemPrivate> d_ptr;
};
