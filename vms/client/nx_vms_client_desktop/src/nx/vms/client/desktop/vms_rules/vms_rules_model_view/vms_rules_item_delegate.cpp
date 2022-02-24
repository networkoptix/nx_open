// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_item_delegate.h"

#include <QtGui/QPainter>
#include <QtWidgets/QStyle>
#include <QtWidgets/QApplication>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/vms_rules/vms_rules_model_view/vms_rules_list_model.h>

namespace
{

static constexpr auto kModificationMarkColumnWidth = 72;
static constexpr auto kSubItemSpacing = 8;

} // namespace

namespace nx::vms::client::desktop {
namespace vms_rules {

VmsRulesItemDelegate::VmsRulesItemDelegate(QObject* parent):
    base_type(parent)
{
}

QSize VmsRulesItemDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    auto result = base_type::sizeHint(option, index);
    result.setHeight(style::Metrics::kViewRowHeight);
    return result;
}

int VmsRulesItemDelegate::modificationMarkColumnWidth()
{
    return kModificationMarkColumnWidth;
}

void VmsRulesItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    const auto modificationMarkRect = styleOption.rect
        .adjusted(styleOption.rect.width() - modificationMarkColumnWidth(), 0, 0, 0);

    const auto eventActionRect = styleOption.rect
        .adjusted(0, 0, -modificationMarkColumnWidth(), 0);

    const auto eventRect = eventActionRect.adjusted(0, 0, -eventActionRect.width() / 2, 0);
    const auto actionRect = eventActionRect.adjusted(eventActionRect.width() / 2, 0, 0, 0);

    auto option = styleOption;
    option.state.setFlag(QStyle::State_HasFocus, false);

    base_type::paint(painter, option, index);
    option.state.setFlag(QStyle::State_MouseOver, false);
    option.state.setFlag(QStyle::State_Selected, false);

    option.rect = eventRect;
    drawEventRepresentation(painter, option, index);

    option.rect = actionRect;
    drawActionRepresentation(painter, option, index);

    option.rect = modificationMarkRect;
    drawModificationMark(painter, option, index);
}

void VmsRulesItemDelegate::drawModificationMark(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    if (!index.data(VmsRulesModelRoles::ruleModificationMarkRole).toBool())
        return;

    painter->save();

    const auto textBarColor = QPalette().color(QPalette::Midlight).lighter(125);
    const auto switchHeight = style::Metrics::kStandaloneSwitchSize.height();
    QRect textBarRect = styleOption.rect.adjusted(8, switchHeight / 2, -8, -switchHeight / 2);

    painter->setBrush(textBarColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(textBarRect, 2, 2);

    static constexpr int kTextMargin = 2;
    const QString modificationMarkText = tr("Not saved").toUpper();
    const auto textColor = QPalette().color(QPalette::Shadow);

    auto font = styleOption.font;
    font.setLetterSpacing(QFont::PercentageSpacing, 110);
    font.setWeight(QFont::DemiBold);
    font.setPixelSize(8);

    painter->setFont(font);
    painter->setPen(textColor);
    painter->setRenderHint(QPainter::TextAntialiasing);

    QTextOption textOption;
    textOption.setAlignment(Qt::AlignCenter);
    painter->drawText(textBarRect, modificationMarkText, textOption);

    painter->restore();
}

void VmsRulesItemDelegate::drawEventRepresentation(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    const auto style = Style::instance();
    if (!style)
        return;

    const QWidget* widget = qobject_cast<QWidget*>(parent());

    const auto eventDescriptionData = index.data(VmsRulesModelRoles::eventDescriptionTextRole);
    const auto eventParametersIconData = index.data(VmsRulesModelRoles::eventParameterIconRole);
    const auto eventParametersTextData = index.data(VmsRulesModelRoles::eventParameterTextRole);

    QFontMetrics fontMetrics(styleOption.font);
    const int eventDescriptionWidth =
        painter->boundingRect(styleOption.rect, eventDescriptionData.toString()).width()
        + kSubItemSpacing;

    // Event description.
    {
        QStyleOptionViewItem opt = styleOption;
        opt.rect.adjust(0, 0, -opt.rect.width() + eventDescriptionWidth, 0);
        opt.text = eventDescriptionData.toString();
        opt.textElideMode = Qt::ElideNone;
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    }

    // Event parameters.
    {
        QStyleOptionViewItem opt = styleOption;
        opt.rect.adjust(eventDescriptionWidth, 0, 0, 0);
        opt.font.setWeight(QFont::DemiBold);
        opt.text = eventParametersTextData.toString();
        if (!eventParametersIconData.isNull())
        {
            opt.features.setFlag(QStyleOptionViewItem::ViewItemFeature::HasDecoration, true);
            opt.features.setFlag(QStyleOptionViewItem::ViewItemFeature::HasDisplay, true);
            opt.icon = eventParametersIconData.value<QIcon>();
        }
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    }
}

void VmsRulesItemDelegate::drawActionRepresentation(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    const auto style = Style::instance();
    if (!style)
        return;

    const QWidget* widget = qobject_cast<QWidget*>(parent());

    const auto actionDescriptionData = index.data(VmsRulesModelRoles::actionDescriptionTextRole);
    const auto actionParametersIconData = index.data(VmsRulesModelRoles::actionParameterIconRole);
    const auto actionParametersTextData = index.data(VmsRulesModelRoles::actionParameterTextRole);

    QFontMetrics fontMetrics(styleOption.font);
    const int actionDescriptionWidth =
        painter->boundingRect(styleOption.rect, actionDescriptionData.toString()).width()
        + kSubItemSpacing;

    // Action description.
    {
        QStyleOptionViewItem opt = styleOption;
        opt.rect.adjust(0, 0, -opt.rect.width() + actionDescriptionWidth, 0);
        opt.text = actionDescriptionData.toString();
        opt.textElideMode = Qt::ElideNone;
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    }

    // Action parameters.
    {
        QStyleOptionViewItem opt = styleOption;
        opt.rect.adjust(actionDescriptionWidth, 0, 0, 0);
        opt.font.setWeight(QFont::DemiBold);
        opt.text = actionParametersTextData.toString();
        if (!actionParametersIconData.isNull())
        {
            opt.features.setFlag(QStyleOptionViewItem::ViewItemFeature::HasDecoration, true);
            opt.features.setFlag(QStyleOptionViewItem::ViewItemFeature::HasDisplay, true);
            opt.icon = actionParametersIconData.value<QIcon>();
        }
       style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    }
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
