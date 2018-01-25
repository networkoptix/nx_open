#include "notification_counter_label.h"

#include <ui/style/helper.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kFontPixelSize = 9;
static constexpr int kFontWeight = QFont::Bold;
static constexpr int kFixedHeight = 12;
static const QMargins kContentsMargins(3, 1, 3, 0); //< Manually adjusted for the best appearance.

} // namespace

NotificationCounterLabel::NotificationCounterLabel(QWidget* parent): base_type(parent)
{
    setFixedHeight(kFixedHeight);
    setMinimumWidth(kFixedHeight);
    setRoundingRadius(kFixedHeight / 2.0, false);
    setContentsMargins(kContentsMargins);
    setProperty(style::Properties::kDontPolishFontProperty, true);

    QFont font(this->font());
    font.setPixelSize(kFontPixelSize);
    font.setWeight(kFontWeight);
    setFont(font);
}

} // namespace desktop
} // namespace client
} // namespace nx
