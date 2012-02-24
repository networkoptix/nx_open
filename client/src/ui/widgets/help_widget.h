#ifndef QN_HELP_WIDGET_H
#define QN_HELP_WIDGET_H

#include <QWidget>
#include <QScopedPointer>
#include <help/qncontext_help.h>

namespace Ui {
    class HelpWidget;
}

class QnHelpWidget: public QWidget {
    Q_OBJECT;
public:
    QnHelpWidget(QnContextHelp *contextHelp, QWidget *parent = NULL);
    virtual ~QnHelpWidget();

signals:
    void showRequested();

protected slots:
    void at_contextHelp_helpContextChanged(QnContextHelp::ContextId id);
    void at_checkBox_toggled(bool checked);

protected:
    virtual void wheelEvent(QWheelEvent *event) override;

    QnContextHelp *contextHelp() const {
        return m_contextHelp.data();
    }

private:
    QScopedPointer<Ui::HelpWidget> ui;
    QWeakPointer<QnContextHelp> m_contextHelp;
    QnContextHelp::ContextId m_id;
};

#endif // QN_HELP_WIDGET_H
