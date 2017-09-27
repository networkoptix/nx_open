#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class SelectableTextButton;

// An object that ensures no more than one selectable button
// from a specified set is selected at any moment.
// If selectionFallback is set it also ensures that
// precisely one button is selected at any moment.

class SelectableTextButtonGroup: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit SelectableTextButtonGroup(QObject* parent = nullptr);
    virtual ~SelectableTextButtonGroup() override;

    bool add(SelectableTextButton* button);
    bool remove(SelectableTextButton* button);

    SelectableTextButton* selected() const;
    bool setSelected(SelectableTextButton* button);

    // If selectionFallback is set, setSelected(nullptr) will select the fallback instead.
    SelectableTextButton* selectionFallback() const;
    bool setSelectionFallback(SelectableTextButton* value);

signals:
    void selectionChanged(SelectableTextButton* oldButton, SelectableTextButton* newButton);
        //< oldButton might be already destroyed.

    void buttonStateChanged(SelectableTextButton* button);

private:
    QSet<SelectableTextButton*> m_buttons;
    SelectableTextButton* m_selected = nullptr;
    QPointer<SelectableTextButton> m_selectionFallback;
    bool m_selectionChanging = false;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
