// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QTextEdit>

namespace nx::vms::client::desktop {

/**
 * QLabel supports text wrapping at word boundary but not any arbitrary place.
 * This QTextEdit descendant serves to overcome that limitation.
 */
class TextEditLabel: public QTextEdit
{
    Q_OBJECT
    using base_type = QTextEdit;

public:
    TextEditLabel(QWidget* parent = nullptr);

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    bool isAutoWrapped() const;

private:
    QSize m_documentSize;
};

} // namespace nx::vms::client::desktop
