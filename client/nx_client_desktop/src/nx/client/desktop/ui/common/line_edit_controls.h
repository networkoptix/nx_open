#pragma once

#include <initializer_list>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

class QLineEdit;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

// A class that allows adding custom widgets to QLineEdit to the right of its text.
// Replaces QLineEdit built-in actions system which has very low flexibility and usability.
//
// Custom widgets are added by addControl(s) and aligned from left to right.
// Line edit's layout direction is honored, in case of RTL direction widgets are aligned from
// right to left and the entire block is sticked to the left side of line edit.
//
// If custom widget has QSizePolicy::IgnoreFlag in its horizontal or vertical size policy,
// then it's width or height respectively is kept unchanged, otherwise the widget is resized to
// width or height of it's sizeHint constrained by minimumSizeHint, minimimSize and maximumSize.
// If the widget has QSizePolicy::ExpandFlag in its vertical size policy, it's expanded to
// full available height. QSizePolicy::ExpandFlag in horizontal size policy is ignored.
//
// Margins around custom widgets block are controlled with inherited contentsMargins property.
// Spacing between widgets is controlled with spacing property.

class LineEditControls: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing)
    using base_type = QWidget;

    LineEditControls(QLineEdit* lineEdit);

public:
    static LineEditControls* get(QLineEdit* lineEdit);
    virtual ~LineEditControls() override;

    void addControl(QWidget* control);
    void addControls(const std::initializer_list<QWidget*>& controls);

    int spacing() const;
    void setSpacing(int value);

protected:
    virtual bool event(QEvent* event) override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
