// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shortcut_hint_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/build_info.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/common/widgets/key_hint_label.h>

namespace {

QString keyToString(Qt::Key key)
{
    switch (key)
    {
        case Qt::Key_Enter:
            return "Enter";

        case Qt::Key_Shift:
            return nx::build_info::isMacOsX()
                ? QKeySequence(key).toString(QKeySequence::NativeText)
                : "Shift";

        case Qt::Key_Control:
            return nx::build_info::isMacOsX()
                ? QKeySequence(key).toString(QKeySequence::NativeText)
                : "Ctrl";
        default:
            return QKeySequence(key).toString(QKeySequence::NativeText);
    }
}

QString placeholderString(int placeholderIndex)
{
    return QString("%") + QString::number(placeholderIndex);
}

bool isPlaceholder(const QString& string)
{
    bool ok = false;
    if (string.size() < 2 || !string.startsWith("%"))
        return false;

    string.mid(1).toInt(&ok);
    return ok;
}

int placeholderIndex(const QString& string)
{
    return string.mid(1).toInt();
}

QStringList splitString(const QString& string)
{
    if (string.isEmpty())
        return {};

    std::vector<int> breakPoints;
    breakPoints.push_back(0);

    int stringPos = 0;
    while (stringPos < string.size())
    {
        auto placeholderPos = string.indexOf(QChar('%'), stringPos);
        if (placeholderPos == -1)
            break;

        auto placeholderSize = 1;
        while (placeholderPos + placeholderSize < string.size()
            && string.at(placeholderPos + placeholderSize).isDigit())
        {
            ++placeholderSize;
        }

        if (placeholderSize > 1)
        {
            if (placeholderPos != 0)
                breakPoints.push_back(placeholderPos);
            if (placeholderPos + placeholderSize < string.size())
                breakPoints.push_back(placeholderPos + placeholderSize);
        }
        stringPos = placeholderPos + placeholderSize;
    }
    breakPoints.push_back(string.size());

    QStringList result;
    for (int i = 0; i < breakPoints.size() - 1; ++i)
    {
        const auto subsringSize = breakPoints.at(i + 1) - breakPoints.at(i);
        result.push_back(string.mid(breakPoints.at(i), subsringSize));
    }

    return result;
}

} // namespace

namespace nx::vms::client::desktop {

ShortcutHintBuilder::ShortcutHintBuilder(ShortcutHintWidget* hintWidget):
    m_hintWidget(hintWidget)
{
}

ShortcutHintBuilder::~ShortcutHintBuilder()
{
    m_hintWidget->clean();
    for (const auto& string: std::as_const(m_splittedString))
    {
        if (isPlaceholder(string) && placeholderIndex(string) < (m_keySequences.size() + 1))
            m_hintWidget->addKeySequence(m_keySequences.at(placeholderIndex(string) - 1));
        else
            m_hintWidget->addLabel(string);
    }
}

ShortcutHintBuilder& ShortcutHintBuilder::withHintLine(
    const QString& hintString)
{
    m_splittedString = splitString(hintString);
    return *this;
}

ShortcutHintBuilder& ShortcutHintBuilder::withKeySequence(
    const QKeySequence& keySequence)
{
    m_keySequences.push_back(keySequence);
    return *this;
}

ShortcutHintWidget::ShortcutHintWidget(QWidget* parent):
    base_type(parent)
{
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(QMargins(0, 0, 0, 0));
    layout->setSpacing(fontMetrics().horizontalAdvance(QChar::Space));
}

void ShortcutHintWidget::clean()
{
    while (layout()->count() > 1)
        layout()->removeItem(layout()->itemAt(0));
}

void ShortcutHintWidget::addKeySequence(const QKeySequence& keySequence)
{
    for (int i = 0; i < keySequence.count(); ++i)
    {
        const auto keyString = keyToString(keySequence[i].key());
        layout()->addWidget(new KeyHintLabel(keyString.trimmed()));
        if (i < keySequence.count() - 1)
            layout()->addWidget(new QLabel("+"));
    }
}

void ShortcutHintWidget::addLabel(const QString& string)
{
    layout()->addWidget(new QLabel(string));
}

} // namespace nx::vms::client::desktop
