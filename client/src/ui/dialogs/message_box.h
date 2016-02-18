#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>

#include <ui/dialogs/dialog.h>

class Ui_QnMessageBox;
class QnMessageBoxPrivate;

class QnMessageBox: public QnDialog
{
    Q_OBJECT
    typedef QnDialog base_type;

public:
    enum Icon
    {
        NoIcon = 0,
        Information,
        Warning,
        Critical,
        Question
    };

public:

    explicit QnMessageBox(QWidget *parent = nullptr);

    QnMessageBox(
            Icon icon,
            int helpTopicId,
            const QString &title,
            const QString &text,
            QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::NoButton,
            QWidget *parent = nullptr,
            Qt::WindowFlags flags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

    ~QnMessageBox();

    void addButton(QAbstractButton *button, QDialogButtonBox::ButtonRole role);
    QPushButton *addButton(const QString &text, QDialogButtonBox::ButtonRole role);
    QPushButton *addButton(QDialogButtonBox::StandardButton button);
    void removeButton(QAbstractButton *button);

    QList<QAbstractButton *> buttons() const;
    QDialogButtonBox::ButtonRole buttonRole(QAbstractButton *button) const;

    void setStandardButtons(QDialogButtonBox::StandardButtons buttons);
    QDialogButtonBox::StandardButtons standardButtons() const;
    QDialogButtonBox::StandardButton standardButton(QAbstractButton *button) const;
    QPushButton *button(QDialogButtonBox::StandardButton which) const;

    QPushButton *defaultButton() const;
    void setDefaultButton(QPushButton *button);
    void setDefaultButton(QDialogButtonBox::StandardButton button);

    QAbstractButton *escapeButton() const;
    void setEscapeButton(QAbstractButton *button);
    void setEscapeButton(QDialogButtonBox::StandardButton button);

    QAbstractButton *clickedButton() const;

    QString text() const;
    void setText(const QString &text);

    Icon icon() const;
    void setIcon(Icon);

    Qt::TextFormat textFormat() const;
    void setTextFormat(Qt::TextFormat format);

    QString informativeText() const;
    void setInformativeText(const QString &text);

    QString checkBoxText() const;
    void setCheckBoxText(const QString &text);
    bool isChecked() const;
    void setChecked(bool checked);

    virtual int exec() override;

    static QDialogButtonBox::StandardButton information(
            QWidget *parent,
            const QString &title,
            const QString &text,
            QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
            QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);
    static QDialogButtonBox::StandardButton information(
            QWidget *parent,
            int helpTopicId,
            const QString &title,
            const QString &text,
            QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
            QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);

    static QDialogButtonBox::StandardButton question(
            QWidget *parent,
            const QString &title,
            const QString &text,
            QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
            QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);
    static QDialogButtonBox::StandardButton question(
            QWidget *parent,
            int helpTopicId,
            const QString &title,
            const QString &text,
            QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
            QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);

    static QDialogButtonBox::StandardButton warning(
            QWidget *parent,
            const QString &title,
            const QString &text,
            QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
            QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);
    static QDialogButtonBox::StandardButton warning(
            QWidget *parent,
            int helpTopicId,
            const QString &title,
            const QString &text,
            QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
            QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);

    static QDialogButtonBox::StandardButton critical(
            QWidget *parent,
            const QString &title,
            const QString &text,
            QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
            QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);
    static QDialogButtonBox::StandardButton critical(
            QWidget *parent,
            int helpTopicId,
            const QString &title,
            const QString &text,
            QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
            QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);

protected:
    virtual void closeEvent(QCloseEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;

private:
    QScopedPointer<Ui_QnMessageBox> ui;

    Q_DECLARE_PRIVATE(QnMessageBox)
    QScopedPointer<QnMessageBoxPrivate> d_ptr;
};
