#ifndef DIALOG_BUTTON_BOX_H
#define DIALOG_BUTTON_BOX_H

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>

class QnProgressWidget : public QWidget {
    Q_OBJECT

public:
    QnProgressWidget(QWidget *parent = 0);

    void setText(const QString &text);
private:
    QLabel* m_img;
    QLabel* m_text;
};

class QnDialogButtonBox : public QDialogButtonBox {
    Q_OBJECT

    typedef QDialogButtonBox base_type;
public:
    QnDialogButtonBox(QWidget *parent = 0);
    QnDialogButtonBox(Qt::Orientation orientation, QWidget *parent = 0);
    explicit QnDialogButtonBox(StandardButtons buttons, QWidget *parent = 0);
    QnDialogButtonBox(StandardButtons buttons, Qt::Orientation orientation,
                     QWidget *parent = 0);
    ~QnDialogButtonBox();

public slots:
    void showProgress(const QString &text = QString());
    void hideProgress();

private:
    QnProgressWidget* progressWidget(bool canCreate = false);

};

#endif // DIALOG_BUTTON_BOX_H
