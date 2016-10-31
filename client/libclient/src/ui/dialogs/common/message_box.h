#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>

#include <ui/dialogs/common/dialog.h>


namespace Ui {
class MessageBox;
}

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
        Question,
        Success
    };

    /* Positions of custom widgets in the message box. */
    enum class Layout
    {
        Main,
        Content
    };

public:

    QnMessageBox(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

    QnMessageBox(
            Icon icon,
            int helpTopicId,
            const QString &title,
            const QString &text,
            QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::NoButton,
            QWidget *parent = nullptr,
            Qt::WindowFlags flags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

    virtual ~QnMessageBox();

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

    QAbstractButton *defaultButton() const;
    void setDefaultButton(QAbstractButton *button);
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

    /** Informative text, that will be split by \n to several paragraphs */
    void setInformativeText(const QString &text, bool split = true);

    /** Delegate widget with custom details. QnMessageBox will take ownership. */
    void addCustomWidget(QWidget* widget, Layout layout = Layout::Content, int stretch = 0,
        Qt::Alignment alignment = Qt::Alignment(), bool focusThisWidget = false);

    /**
     * Widget will be removed from layout, but you should manually hide and delete it if required.
     * Dialog will still retain ownership.
     */
    void removeCustomWidget(QWidget* widget);

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

    static QDialogButtonBox::StandardButton success(
        QWidget *parent,
        const QString &title,
        const QString &text,
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);
    static QDialogButtonBox::StandardButton success(
        QWidget *parent,
        int helpTopicId,
        const QString &title,
        const QString &text,
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void closeEvent(QCloseEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;

    virtual void afterLayout() override;

private:
    QScopedPointer<Ui::MessageBox> ui;

    Q_DECLARE_PRIVATE(QnMessageBox)
    QScopedPointer<QnMessageBoxPrivate> d_ptr;
};
