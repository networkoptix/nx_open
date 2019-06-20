#pragma once

#include <functional>

#include <QtCore/QScopedPointer>
#include <QtGui/QIcon>
#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

class EditableLabel: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(bool editing READ editing WRITE setEditing DESIGNABLE false)
    Q_PROPERTY(QIcon buttonIcon READ buttonIcon WRITE setButtonIcon)
    Q_PROPERTY(bool readOnly READ readOnly WRITE setReadOnly)

    using base_type = QWidget;

public:
    explicit EditableLabel(QWidget* parent = nullptr);
    virtual ~EditableLabel();

    QString text() const;
    void setText(const QString& text);

    bool editing() const;
    void setEditing(bool editing, bool applyChanges = true);

    QIcon buttonIcon() const;
    void setButtonIcon(const QIcon& icon);

    bool readOnly() const;
    void setReadOnly(bool readOnly);

    using Validator = std::function<bool (QString&)>;
    void setValidator(Validator validator);

signals:
    /**
     * After editing has been started.
     */
    void editingStarted();

    /**
     * After editing has been finished.
     */
    void editingFinished();

    /** After editing has been finished and resulted in a new text,
     * or new text has been set programmatically while editing is off.
     */
    void textChanged(const QString& text);

    /** After edit box contents has been changed during editing
     * by user input or programmatically.
     */
    void textChanging(const QString& text);

protected:
    virtual void changeEvent(QEvent* event) override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
