#ifndef QN_CHECKABLE_MESSAGE_BOX_H
#define QN_CHECKABLE_MESSAGE_BOX_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtCore/QScopedPointer>

class QnCheckableMessageBoxPrivate;

/**
 * \brief A messagebox suitable for questions with a "Do not ask me again" checkbox.
 *
 * Emulates the QDialogButtonBox API with static conveniences. 
 * The message label can open external URLs.
 * If user cancel the dialog (e.g. with "Esc" button) cancelButton will be returned as a result.
 */
class QnCheckableMessageBox: public QDialog {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QPixmap iconPixmap READ iconPixmap WRITE setIconPixmap)
    Q_PROPERTY(bool isChecked READ isChecked WRITE setChecked)
    Q_PROPERTY(QString checkBoxText READ checkBoxText WRITE setCheckBoxText)
    Q_PROPERTY(QDialogButtonBox::StandardButtons buttons READ standardButtons WRITE setStandardButtons)
    Q_PROPERTY(QDialogButtonBox::StandardButton defaultButton READ defaultButton WRITE setDefaultButton)

public:
    explicit QnCheckableMessageBox(QWidget *parent);
    virtual ~QnCheckableMessageBox();

    static QDialogButtonBox::StandardButton
    question(QWidget *parent,
        int helpTopicId,
        const QString &title,
        const QString &question,
        const QString &checkBoxText,
        bool *checkBoxSetting,
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Yes | QDialogButtonBox::No,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::No,
        QDialogButtonBox::StandardButton cancelButton = QDialogButtonBox::No);

    static QDialogButtonBox::StandardButton
    question(QWidget *parent,
        const QString &title,
        const QString &question,
        const QString &checkBoxText,
        bool *checkBoxSetting,
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Yes | QDialogButtonBox::No,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::No,
        QDialogButtonBox::StandardButton cancelButton = QDialogButtonBox::No);

    static QDialogButtonBox::StandardButton
    warning(QWidget *parent,
        int helpTopicId,
        const QString &title,
        const QString &warning,
        const QString &checkBoxText,
        bool *checkBoxSetting,
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton cancelButton = QDialogButtonBox::Cancel);

    static QDialogButtonBox::StandardButton
    warning(QWidget *parent,
        const QString &title,
        const QString &warning,
        const QString &checkBoxText,
        bool *checkBoxSetting,
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton cancelButton = QDialogButtonBox::Cancel);

    QString text() const;
    void setText(const QString &);
    void setRichText(const QString &);

    bool isChecked() const;
    void setChecked(bool s);

    QString checkBoxText() const;
    void setCheckBoxText(const QString &);

    bool isCheckBoxVisible() const;
    void setCheckBoxVisible(bool);

    QDialogButtonBox::StandardButtons standardButtons() const;
    void setStandardButtons(QDialogButtonBox::StandardButtons s);
    QPushButton *button(QDialogButtonBox::StandardButton b) const;
    QPushButton *addButton(const QString &text, QDialogButtonBox::ButtonRole role);

    QDialogButtonBox::StandardButton defaultButton() const;
    void setDefaultButton(QDialogButtonBox::StandardButton s);

    QPixmap iconPixmap() const;
    void setIconPixmap(const QPixmap &p);

    QAbstractButton *clickedButton() const;
    QDialogButtonBox::StandardButton clickedStandardButton(QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton) const;

private slots:
    void at_buttonBox_clicked(QAbstractButton *b);

private:
    QScopedPointer<QnCheckableMessageBoxPrivate> d;
};

#endif // QN_CHECHABLE_MESSAGE_BOX_H
