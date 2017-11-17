#pragma once

#include <QtCore/QString>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>

#include <client/client_globals.h>

#include <ui/dialogs/common/dialog.h>

namespace Ui {
class MessageBox;
}

class QnMessageBoxPrivate;

enum class QnMessageBoxIcon
{
    NoIcon,
    Information,
    Warning,
    Critical,
    Question,
    Success
};

enum class QnButtonDetection
{
    NoDetection = 0x0,
    DefaultButton = 0x1
};
Q_DECLARE_FLAGS(QnButtonDetections, QnButtonDetection)
Q_DECLARE_OPERATORS_FOR_FLAGS(QnButtonDetections)

enum class QnMessageBoxCustomButton
{
    Overwrite,                  //< QDialogButtonBox::AcceptRole / Qn::ButtonAccent::Warning
    Delete,                     //< QDialogButtonBox::AcceptRole / Qn::ButtonAccent::Warning
    Reset,                      //< QDialogButtonBox::AcceptRole / Qn::ButtonAccent::Warning
    Skip,                       //< QDialogButtonBox::RejectRole / Qn::ButtonAccent::NoAccent
};

class QnMessageBox: public QnDialog
{
    Q_OBJECT
    typedef QnDialog base_type;

public:
    /* Positions of custom widgets in the message box. */
    enum class Layout
    {
        Main,
        Content,
        AfterMainLabel
    };

public:
    QnMessageBox(QWidget* parent);

    QnMessageBox(
        QnMessageBoxIcon icon,
        const QString& text,
        const QString& extras,
        QDialogButtonBox::StandardButtons buttons,
        QDialogButtonBox::StandardButton defaultButton,
        QWidget* parent);

    static QDialogButtonBox::StandardButton information(
        QWidget* parent,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);

    static QDialogButtonBox::StandardButton warning(
        QWidget* parent,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);

    static QDialogButtonBox::StandardButton question(
        QWidget* parent,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);

    static QDialogButtonBox::StandardButton critical(
        QWidget* parent,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);

    static QDialogButtonBox::StandardButton success(
        QWidget* parent,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);

public:
    virtual ~QnMessageBox();

    QPushButton* addCustomButton(
        QnMessageBoxCustomButton button,
        QDialogButtonBox::ButtonRole role,
        Qn::ButtonAccent accent = Qn::ButtonAccent::NoAccent);

    void addButton(QAbstractButton *button, QDialogButtonBox::ButtonRole role);
    QPushButton* addButton(
        const QString &text,
        QDialogButtonBox::ButtonRole role,
        Qn::ButtonAccent accent = Qn::ButtonAccent::NoAccent);
    QPushButton* addButton(QDialogButtonBox::StandardButton button);
    void removeButton(QAbstractButton *button);

    QList<QAbstractButton *> buttons() const;
    QDialogButtonBox::ButtonRole buttonRole(QAbstractButton *button) const;

    void setStandardButtons(QDialogButtonBox::StandardButtons buttons);
    QDialogButtonBox::StandardButtons standardButtons() const;
    QDialogButtonBox::StandardButton standardButton(QAbstractButton *button) const;
    QPushButton *button(QDialogButtonBox::StandardButton which) const;

    QAbstractButton *defaultButton() const;
    void setDefaultButton(
        QAbstractButton *button,
        Qn::ButtonAccent accent = Qn::ButtonAccent::Standard);

    void setDefaultButton(
        QDialogButtonBox::StandardButton button,
        Qn::ButtonAccent accent = Qn::ButtonAccent::Standard);

    QAbstractButton *escapeButton() const;
    void setEscapeButton(QAbstractButton *button);
    void setEscapeButton(QDialogButtonBox::StandardButton button);

    QAbstractButton *clickedButton() const;

    QString text() const;
    void setText(const QString &text);

    QnMessageBoxIcon icon() const;
    void setIcon(QnMessageBoxIcon icon);

    Qt::TextFormat textFormat() const;
    void setTextFormat(Qt::TextFormat format);

    Qt::TextFormat informativeTextFormat() const;
    void setInformativeTextFormat(Qt::TextFormat format);

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

    void setCustomCheckBoxText(const QString &text);
    bool isCheckBoxEnabled() const;
    void setCheckBoxEnabled(bool value = true);

    bool isChecked() const;
    void setChecked(bool checked);


    void setButtonAutoDetection(QnButtonDetection detection);

    virtual int exec() override;

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void closeEvent(QCloseEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;

    virtual void afterLayout() override;

private:
    static QDialogButtonBox::StandardButton execute(
        QWidget* parent,
        QnMessageBoxIcon icon,
        const QString& text,
        const QString& extras = QString(),
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok,
        QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);

    void initialize();

    QPixmap getPixmapByIconId(QnMessageBoxIcon icon);

private:
    const QScopedPointer<Ui::MessageBox> ui;

    Q_DECLARE_PRIVATE(QnMessageBox)
    const QScopedPointer<QnMessageBoxPrivate> d_ptr;
};
