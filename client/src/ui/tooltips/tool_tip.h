#ifndef QN_TOOL_TIP_H
#define QN_TOOL_TIP_H

#include <QObject>

class QLabel;
class QGraphicsView;

/**
 * This class provides advanced tooltip handling.
 */
class QnToolTip: public QObject {
    Q_OBJECT;

public:
    QnToolTip();

    virtual ~QnToolTip();

    static QnToolTip *instance();

    void registerToolTip(QLabel *label);

    void unregisterToolTip(QLabel *label);

    void registerGraphicsView(QGraphicsView *view);

    void unregisterGraphicsView(QGraphicsView *view);

protected:
    virtual bool eventFilter(QObject *watched, QEvent *event) override;
    bool widgetEventFilter(QObject *watched, QEvent *event);
    bool toolTipEventFilter(QObject *watched, QEvent *event);

protected slots:
    void at_graphicsView_destroyed(QObject *object);

private:
    QCoreApplication *m_application;
    QObject *m_toolTipEventFilter, *m_widgetEventFilter;
    QSet<QLabel *> m_toolTips;
    QSet<QGraphicsView *> m_graphicsViews;
};


#endif // QN_TOOL_TIP_H
