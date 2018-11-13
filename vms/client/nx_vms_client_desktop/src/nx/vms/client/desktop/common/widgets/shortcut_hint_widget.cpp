#include "shortcut_hint_widget.h"

#include <QtGui/QFont>

#include <nx/utils/app_info.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace {

using namespace nx::vms::client::desktop;

QString makeLeftFloat(const QString& text)
{
    static const auto kLeftFloatTag =
        lit("<table cellpadding=2 style=\"float: left\"><tr><td>%1</td></tr></table>");
    return kLeftFloatTag.arg(text);
}


QString keyToString(int key)
{
    switch (key)
    {
        case Qt::Key_Enter:
            return lit("Enter");
        case Qt::Key_Control:
            return nx::utils::AppInfo::isMacOsX()
                ? QKeySequence(key).toString(QKeySequence::NativeText)
                : lit("Ctrl");
        default:
            return QKeySequence(key).toString(QKeySequence::NativeText);
    }
}

QString getHintItemText(const nx::vms::client::desktop::ShortcutHintWidget::Description& description)
{
    static const auto kBorderColor = colorTheme()->color("dark8").name(QColor::HexArgb);

    static const auto kHtmlBorder = lit(
        "<table cellspacing=-1 cellpadding=2 border=1"
        "   style=\"float:left;border-style:solid;border-color:%2;\">"
        "<tr><td>&nbsp;%1&nbsp;</td></tr>"
        "</table>");

    const auto& keySequence = description.first;
    const auto count = keySequence.count();
    QStringList keyItemTexts;
    for (int keyIndex = 0; keyIndex != count; ++keyIndex)
    {
        const auto key = keySequence[keyIndex];
        const auto textKey = keyToString(key);
        keyItemTexts.append(kHtmlBorder.arg(textKey, kBorderColor));
    }

    static const auto kPlusSeparator = makeLeftFloat(lit("+"));
    static const auto kMDash = makeLeftFloat(lit("&nbsp;&mdash;"));
    const auto keys = keyItemTexts.join(kPlusSeparator);
    const auto hint = makeLeftFloat(description.second);
    return lit("<table width=100% border = 0 border-radius = 3><tr><td>%1 %3 %2</td></tr></table>")
        .arg(keys, hint, kMDash);
}

} // namespace

namespace nx::vms::client::desktop {

ShortcutHintWidget::ShortcutHintWidget(QWidget* parent):
    base_type(parent)
{
    setTextFormat(Qt::RichText);
    auto currentFont = font();
    currentFont.setPixelSize(12);
    setFont(currentFont);
    setAlignment(Qt::AlignTop);
}

void ShortcutHintWidget::setDescriptions(const DescriptionList& descriptions)
{
    static const auto kTextColor = colorTheme()->color("dark14")
        .name(QColor::HexArgb);
    static const auto kHtmlTemplate =
        lit("<html><body><center>"
            "   <font color=%1>%2</font>"
            "</center></body></html>").arg(kTextColor);

    QStringList items;
    for (const auto& description: descriptions)
        items.append(getHintItemText(description));

    setText(kHtmlTemplate.arg(items.join(QString())));
}

} // namespace nx::vms::client::desktop

