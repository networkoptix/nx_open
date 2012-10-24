#ifndef QN_HELP_WIDGET_H
#define QN_HELP_WIDGET_H

#include <QWidget>
#include <QScopedPointer>
#include <ui/help/help_handler.h>

namespace Ui {
    class HelpWidget;
}

class QnHelpWidget: public QWidget {
    Q_OBJECT;
public:
    QnHelpWidget(QnHelpHandler *contextHelp, QWidget *parent = NULL);
    virtual ~QnHelpWidget();

signals:
    void showRequested();
    void hideRequested();

protected slots:
    void at_contextHelp_helpContextChanged(int id);
    void at_checkBox_clicked(bool checked);

protected:
    virtual void wheelEvent(QWheelEvent *event) override;

    QnHelpHandler *contextHelp() const {
        return m_contextHelp.data();
    }

private:
    QScopedPointer<Ui::HelpWidget> ui;
    QWeakPointer<QnHelpHandler> m_contextHelp;
    int m_id;
};

#endif // QN_HELP_WIDGET_H
