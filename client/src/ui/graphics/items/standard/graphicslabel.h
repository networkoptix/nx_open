#ifndef GRAPHICSLABEL_H
#define GRAPHICSLABEL_H

#include "graphicsframe.h"

class GraphicsLabelPrivate;

class GraphicsLabel : public GraphicsFrame
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

    typedef GraphicsFrame base_type;

public:
    explicit GraphicsLabel(QGraphicsItem *parent = 0, Qt::WindowFlags f = 0);
    explicit GraphicsLabel(const QString &text, QGraphicsItem *parent = 0, Qt::WindowFlags f = 0);
    ~GraphicsLabel();

    QString text() const;

public Q_SLOTS:
    void setText(const QString &text);
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
