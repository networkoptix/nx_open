#ifndef QN_RECONNECT_INFO_DIALOG_H
#define QN_RECONNECT_INFO_DIALOG_H

#include <QtWidgets/QDialog>

namespace Ui {
    class ReconnectInfoDialog;
}

class QnReconnectInfoDialog: public QDialog {
    Q_OBJECT

    typedef QDialog base_type;
public:
    explicit QnReconnectInfoDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnReconnectInfoDialog();

    QString text() const;
    void setText(const QString &text);

    bool wasCanceled() const;
private:
    Q_DISABLE_COPY(QnReconnectInfoDialog);

    QScopedPointer<Ui::ReconnectInfoDialog> ui;
    bool m_cancelled;
};

#endif //QN_RECONNECT_INFO_DIALOG_H