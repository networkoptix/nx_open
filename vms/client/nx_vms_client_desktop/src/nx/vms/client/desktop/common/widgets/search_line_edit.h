// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <qt_widgets/line_edit.h>

namespace nx::utils { class PendingOperation; }

namespace nx::vms::client::desktop {

class SearchLineEdit: public LineEdit
{
    Q_OBJECT
    Q_PROPERTY(int textChangedSignalFilterMs READ textChangedSignalFilterMs WRITE setTextChangedSignalFilterMs)

public:
    explicit SearchLineEdit(QWidget* parent = nullptr);
    virtual ~SearchLineEdit() override;

    int textChangedSignalFilterMs() const;
    void setTextChangedSignalFilterMs(int filterMs);

    void setGlassVisible(bool visible);

private:
    QAction* m_glassIcon = nullptr;
    const QScopedPointer<nx::utils::PendingOperation> m_emitTextChanged;
};

} // namespace nx::vms::client::desktop
