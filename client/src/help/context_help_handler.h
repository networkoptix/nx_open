#ifndef QN_CONTEXT_HELP_HANDLER_H
#define QN_CONTEXT_HELP_HANDLER_H

#include <QtCore/QObject>
#include <QtCore/QWeakPointer>

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
    QnContextHelpQueryable *queryable(QObject *object);

private:
    QWeakPointer<QnContextHelp> m_contextHelp;
};

#endif // QN_CONTEXT_HELP_HANDLER_H

