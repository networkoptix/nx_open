// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QKeySequence>
#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

class ShortcutHintWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ShortcutHintWidget(QWidget* parent = nullptr);

    void clean();
    void addKeySequence(const QKeySequence& keySequence);
    void addLabel(const QString& string);
};

class ShortcutHintBuilder
{
public:
    ShortcutHintBuilder(ShortcutHintWidget* hintWidget);
    ~ShortcutHintBuilder();

    ShortcutHintBuilder& withHintLine(const QString& hintString);
    ShortcutHintBuilder& withKeySequence(const QKeySequence& keySequence);

private:
    ShortcutHintWidget* m_hintWidget;
    QStringList m_splittedString;
    QVector<QKeySequence> m_keySequences;
};

} // namespace nx::vms::client::desktop
