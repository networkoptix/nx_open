#ifndef QN_HELP_CONTEXT_WIDGET_H
#define QN_HELP_CONTEXT_WIDGET_H

#include <QWidget>
#include <QScopedPointer>
#include <help/qncontext_help.h>

namespace Ui {
    class HelpContextWidget;
}

class QnHelpContextWidget: public QWidget {
    Q_OBJECT;
public:
    QnHelpContextWidget(QnContextHelp *contextHelp, QWidget *parent = NULL);
    virtual ~QnHelpContextWidget();

protected slots:
    void at_contextHelp_helpContextChanged(QnContextHelp::ContextId id);

protected:
    QnContextHelp *contextHelp() const {
        return m_contextHelp.data();
    }

private:
    QScopedPointer<Ui::HelpContextWidget> ui;
    QWeakPointer<QnContextHelp> m_contextHelp;
};

#endif // QN_HELP_CONTEXT_WIDGET_H