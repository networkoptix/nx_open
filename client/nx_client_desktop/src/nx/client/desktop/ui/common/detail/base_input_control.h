#pragma once

#include <QtWidgets/QWidget>

#include <ui/utils/validators.h>
#include <utils/common/connective.h>

class AbstractAccessor;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace detail {

class BaseInputFieldPrivate;
class BaseInputField : public Connective<QWidget>
{
    Q_OBJECT
    using base_type = Connective<QWidget>;

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged USER true)
    Q_PROPERTY(QString hint READ hint WRITE setHint)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

public:
    using AccessorPtr = QSharedPointer<AbstractAccessor>;
    explicit BaseInputField(
        QWidget* inputInstance,
        const AccessorPtr& textAccessor,
        const AccessorPtr& readOnlyAccessor,
        const AccessorPtr& placeholderAccessor,
        QWidget* parent = nullptr);

    virtual ~BaseInputField();

    QWidget* input();

public: // Properties.
    QString title() const;
    void setTitle(const QString& value);

    QString hint() const;
    void setHint(const QString& value);

    QString text() const;
    void setText(const QString& value);
    void clear();

    QString placeholderText() const;
    void setPlaceholderText(const QString& value);

    bool isReadOnly() const;
    void setReadOnly(bool value);

public:
    bool validate();

    /* After a small delay calls validate if input is not being edited. */
    void updateDisplayStateDelayed();

    bool isValid() const;
    QValidator::State lastValidationResult() const;

    void setValidator(
        Qn::TextValidateFunction validator,
        bool validateImmediately = false);
    void reset();

    static AbstractAccessor* createLabelWidthAccessor();

signals:
    void textChanged(const QString& text);

protected:
    void handleInputTextChanged();

private:
    Q_DISABLE_COPY(BaseInputField)
    Q_DECLARE_PRIVATE(BaseInputField)

    friend class LabelWidthAccessor;
    QScopedPointer<BaseInputFieldPrivate> d_ptr;
};

} // namespace detail
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
