#ifndef GRAPHICSLABEL_H
#define GRAPHICSLABEL_H

#include "graphics_frame.h"

#include <QtGui/QStaticText>

class GraphicsLabelPrivate;

class GraphicsLabel : public GraphicsFrame
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_ENUMS(PerformanceHint)
    typedef GraphicsFrame base_type;

public:
    enum PerformanceHint {
        NoCaching,
        ModerateCaching,
        AggressiveCaching,
        PixmapCaching
    };

    explicit GraphicsLabel(QGraphicsItem *parent = 0, Qt::WindowFlags f = 0);
    explicit GraphicsLabel(const QString &text, QGraphicsItem *parent = 0, Qt::WindowFlags f = 0);
    ~GraphicsLabel();

    QString text() const;
    Q_SLOT void setText(const QString &text);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);
    
    PerformanceHint performanceHint() const;
    void setPerformanceHint(PerformanceHint performanceHint);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

public slots:
    inline void setNum(int num) { setText(QString::number(num)); }
    inline void setNum(double num) { setText(QString::number(num)); }
    void clear();

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;

    virtual void changeEvent(QEvent *event) override;

private:
    Q_DISABLE_COPY(GraphicsLabel)
    Q_DECLARE_PRIVATE(GraphicsLabel)
};

#endif // GRAPHICSLABEL_H
