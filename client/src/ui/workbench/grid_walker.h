#ifndef QN_GRID_WALKER_H
#define QN_GRID_WALKER_H

#include <QPoint>
#include <QRect>

class QnGridWalker {
public:
    /**
     * Virtual destructor. 
     */
    virtual ~QnGridWalker() {}

    /**
     * Advances this grid walker and returns the next cell.
     *
     * \returns                         Next grid cell.
     */
    virtual QPoint next() = 0;

    /** 
     * Resets this grid walker to its initial state.
     */
    virtual void reset() = 0;
};


class QnAspectRatioGridWalker: public QnGridWalker {
public:
    QnAspectRatioGridWalker(qreal aspectRatio);

    virtual QPoint next() override;

    virtual void reset() override;

protected:
    void advance();

private:
    qreal m_aspectRatio;
    QRect m_rect;
    QPoint m_pos;
    QPoint m_delta;
};


#endif // QN_GRID_WALKER_H
