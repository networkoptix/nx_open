// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop::rules {

class MultilineElidedLabel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

public:
    explicit MultilineElidedLabel(QWidget* parent = nullptr);

    void setText(const QString& text);
    const QString& text() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_text;
};

} // namespace nx::vms::client::desktop::rules
