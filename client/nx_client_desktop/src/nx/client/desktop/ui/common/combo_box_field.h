#pragma once

#include <nx/client/desktop/ui/common/detail/base_input_field.h>

class QComboBox;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class ComboBoxField: public detail::BaseInputField
{
    Q_OBJECT
    using base_type = detail::BaseInputField;

    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)

public:
    static ComboBoxField* create(
        const QStringList& items,
        int currentIndex,
        const Qn::TextValidateFunction& validator,
        QWidget* parent);

    ComboBoxField(QWidget* parent = nullptr);

    bool setRowHidden(int index, bool value = true);

    void setItems(const QStringList& items);
    QStringList items() const;

    void setCurrentIndex(int index);
    int currentIndex() const;

    virtual bool eventFilter(QObject *object, QEvent *event);

signals:
    void currentIndexChanged();

private:
    ComboBoxField(
        const QStringList& items,
        int currentIndex,
        const Qn::TextValidateFunction& validator,
        QWidget* parent);

    void initialize();

private:
    QComboBox* combobox();
    const QComboBox* combobox() const;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

