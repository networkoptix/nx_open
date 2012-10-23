#ifndef QN_CONTEXT_HELP_HANDLER_H
#define QN_CONTEXT_HELP_HANDLER_H

#include <QtCore/QObject>
#include <QtCore/QWeakPointer>

class QGraphicsItem;
class QGraphicsView;

class QnContextHelp;
class QnContextHelpQueryable;

class QnContextHelpHandler: public QObject {
    Q_OBJECT;
public:
    QnContextHelpHandler(QObject *parent = NULL);
    virtual ~QnContextHelpHandler();

    QnContextHelp *contextHelp() const;
    void setContextHelp(QnContextHelp *contextHelp);

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

private:
    int helpTopicAt(QGraphicsItem *item, const QPointF &pos) const;
    int helpTopicAt(QWidget *widget, const QPoint &pos) const;

    QnContextHelpQueryable *queryable(QGraphicsItem *item) const;
    QnContextHelpQueryable *queryable(QObject *object) const;
    QGraphicsView *view(QObject *object) const;

private:
    QWeakPointer<QnContextHelp> m_contextHelp;
};

#endif // QN_CONTEXT_HELP_HANDLER_H

