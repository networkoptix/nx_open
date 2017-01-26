#include "client_show_once_settings.h"

namespace {

static const QString kClientShowOnceId(lit("client_show_once"));

} // namespace

QnClientShowOnceSettings::QnClientShowOnceSettings(QObject* parent):
    base_type(kClientShowOnceId, parent)
{
}
