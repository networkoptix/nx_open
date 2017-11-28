#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>
#include <functional>

class QnEditableLabelPrivate;

class QnEditableLabel: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(bool editing READ editing WRITE setEditing DESIGNABLE false)
    Q_PROPERTY(QIcon buttonIcon READ buttonIcon WRITE setButtonIcon)
    Q_PROPERTY(bool readOnly READ readOnly WRITE setReadOnly)

    using base_type = QWidget;

public:
    explicit QnEditableLabel(QWidget* parent = nullptr);
    virtual ~QnEditableLabel();

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
    Q_DECLARE_PRIVATE(QnEditableLabel);
    QScopedPointer<QnEditableLabelPrivate> d_ptr;
};
