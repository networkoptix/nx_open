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

    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged USER true)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

public:
    using AccessorPtr = QSharedPointer<AbstractAccessor>;
    explicit BaseInputField(
        QWidget* inputInstance,
        const AccessorPtr& textAccessor = AccessorPtr(),
        const AccessorPtr& readOnlyAccessor = AccessorPtr(),
        const AccessorPtr& placeholderAccessor = AccessorPtr(),
        bool useWarningStyleForControl = true,
        QWidget* parent = nullptr);

    virtual ~BaseInputField();

    QWidget* input() const;

public:
    QString title() const;
    void setTitle(const QString& value);

    void setCustomHint(const QString& hint);

    QString text() const;
    void setText(const QString& value);
    void clear();

    QString placeholderText() const;
    void setPlaceholderText(const QString& value);

    bool isReadOnly() const;
    void setReadOnly(bool value);

    bool isValid() const;

public:
    void setIntermediateResult();

    virtual Qn::ValidationResult calculateValidationResult() const;

    bool validate();

    /* After a small delay calls validate if input is not being edited. */
    void updateDisplayStateDelayed();

    QValidator::State lastValidationResult() const;

    void setValidator(
        Qn::TextValidateFunction validator,
        bool validateImmediately = false);
    void reset();

    static AbstractAccessor* createLabelWidthAccessor();

signals:
    void textChanged(const QString& text);
    void isValidChanged();

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
