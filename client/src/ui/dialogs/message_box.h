#ifndef QN_MESSAGE_BOX_H
#define QN_MESSAGE_BOX_H

#include <QtWidgets/QMessageBox>

class QnMessageBox: public QMessageBox {
    Q_OBJECT
    typedef QMessageBox base_type;

public:
    explicit QnMessageBox(QWidget *parent = NULL);
    QnMessageBox(Icon icon, int helpTopicId, const QString &title, const QString &text, StandardButtons buttons = NoButton, QWidget *parent = NULL, Qt::WindowFlags flags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

    static StandardButton showNoIconDialog(QWidget *parent, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);

    static StandardButton information(QWidget *parent, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton information(QWidget *parent, int helpTopicId, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);

    static StandardButton question(QWidget *parent, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton question(QWidget *parent, int helpTopicId, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);

    static StandardButton warning(QWidget *parent, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton warning(QWidget *parent, int helpTopicId, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);

    static StandardButton critical(QWidget *parent, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton critical(QWidget *parent, int helpTopicId, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);

    /**
     * @brief addCustomButton       Adds a button to the message box if it is valid to do so, and returns the push button.
     *                              Unlike standard buttons, this button will not close message box.
     * @param text                  Button text.
     * @param role                  Button role.
     * @return                      Created button.
     */
    QPushButton *addCustomButton(const QString &text, QMessageBox::ButtonRole role);

    virtual int exec() override;

signals:
    void defaultButtonClicked(QAbstractButton* button);

private:
    QList<QAbstractButton*> m_customButtons;
};

#endif // QN_MESSAGE_BOX_H
