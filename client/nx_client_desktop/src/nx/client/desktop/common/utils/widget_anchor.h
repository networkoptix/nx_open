#pragma once

#include <QtCore/QObject>
#include <QtCore/QMargins>
#include <QtCore/QPointer>
#include <QtCore/qnamespace.h>

class QWidget;

namespace nx {
namespace client {
namespace desktop {

/**
 * Common class to anchor a widget to edges of its parent.
 */
class WidgetAnchor: public QObject
{
    Q_OBJECT
    typedef QObject base_type;

    /** Edges to anchor, default all anchored: */
    Q_PROPERTY(Qt::Edges edges READ edges WRITE setEdges)

    /** Margins to keep at anchored edges, default all zeros: */
    Q_PROPERTY(QMargins margins READ margins WRITE setMargins)

public:
    explicit WidgetAnchor(QWidget* widget);

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
    Qt::Edges m_edges = Qt::Edges(Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge);
    QMargins m_margins = QMargins(0, 0, 0, 0);
    QPointer<QWidget> m_widget;
};

} // namespace desktop
} // namespace client
} // namespace nx
