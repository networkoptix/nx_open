#include "shortcut_hint_widget.h"

namespace {

QString makeLeftFloat(const QString& text)
{
    static const auto kLeftFloatTag =
        lit("<table style=\"float: left\"><tr><td>%1</td></tr></table>");
    return kLeftFloatTag.arg(text);
}


QString keyToString(int key)
{
    return key == Qt::Key_Enter
        ? lit("Enter")
        : QKeySequence(key).toString(QKeySequence::NativeText);
}

QString getHintItemText(const nx::client::desktop::ShortcutHintWidget::Description& description)
{
    static const auto kHtmlBorder = lit(
        "<table cellspacing= -1 border=1"
        "   style=\"float:left;border-style:solid;border-color:yellow;\">"
        "<tr><td>%1</td></tr>"
        "</table>");

    const auto& keySequence = description.first;
    const auto count = keySequence.count();
    QStringList keyItemTexts;
    for (int keyIndex = 0; keyIndex != count; ++keyIndex)
    {
        const auto key = keySequence[keyIndex];
        const auto textKey = keyToString(key);
        keyItemTexts.append(kHtmlBorder.arg(textKey));
    }

    static const auto kPlusSeparator = makeLeftFloat(lit("&nbsp;+&nbsp;"));
    static const auto kMDash = makeLeftFloat(lit("&nbsp;&mdash;&nbsp;")); // FIXME: make mdash
    const auto keys = keyItemTexts.join(kPlusSeparator);
    const auto hint = makeLeftFloat(description.second);
    return lit("<table width=100% border = 0><tr><td>%1 %3 %2</td></tr></table>")
        .arg(keys, hint, kMDash);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

ShortcutHintWidget::ShortcutHintWidget(QWidget* parent):
    base_type(parent)
{
    setTextFormat(Qt::RichText);
}

void ShortcutHintWidget::setDescriptions(const DescriptionList& descriptions)
{
    static const auto kHtmlTemplate= lit("<html><body><center>%1</center></body></html>");

    QStringList items;
    for (const auto description: descriptions)
        items.append(getHintItemText(description));

    setText(kHtmlTemplate.arg(items.join(QString())));
}

} // namespace desktop
} // namespace client
} // namespace nx

