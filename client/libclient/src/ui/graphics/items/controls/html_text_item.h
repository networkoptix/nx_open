#pragma once

#include <QtWidgets/QGraphicsWidget>

#include <nx/fusion/model_functions_fwd.h>

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

QN_FUSION_DECLARE_FUNCTIONS(QnHtmlTextItemOptions, (eq));

#define QnHtmlTextItemOptions_Fields (backgroundColor)(maxWidth)(borderRadius)(horPadding)(vertPadding)(autosize)

class QnHtmlTextItem : public QGraphicsWidget {
    Q_OBJECT

    typedef QGraphicsWidget base_type;
public:
    QnHtmlTextItem(const QString &html = QString()
        , const QnHtmlTextItemOptions &options = QnHtmlTextItemOptions()
        , QGraphicsItem *parent = nullptr);

    ~QnHtmlTextItem();

    QString html() const;
    void setHtml(const QString &html);

    QnHtmlTextItemOptions options() const;
    void setOptions(const QnHtmlTextItemOptions &options);

private:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    Q_DECLARE_PRIVATE(QnHtmlTextItem)
    QScopedPointer<QnHtmlTextItemPrivate> d_ptr;
};
