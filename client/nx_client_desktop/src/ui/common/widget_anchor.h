#pragma once

#include <QtCore/QPointer>

class QWidget;

/**
 * Common class to anchor a widget to edges of its parent.
 */
class QnWidgetAnchor : public QObject
{
    Q_OBJECT
    typedef QObject base_type;

    /** Edges to anchor, default all anchored: */
    Q_PROPERTY(Qt::Edges edges READ edges WRITE setEdges)

    /** Margins to keep at anchored edges, default all zeros: */
    Q_PROPERTY(QMargins margins READ margins WRITE setMargins)

public:
    explicit QnWidgetAnchor(QWidget* widget);

    void setEdges(Qt::Edges anchors);
    Qt::Edges edges() const;

    void setMargins(const QMargins& margins);
    void setMargins(int left, int top, int right, int bottom);
    const QMargins& margins() const;

protected:
    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    void updateGeometry();

private:
    Qt::Edges m_edges;
    QMargins m_margins;
    QPointer<QWidget> m_widget;
};
