#include "client_show_once_settings.h"

namespace {

static const QString kClientShowOnceId(lit("show_once"));

} // namespace

QnClientShowOnceSettings::QnClientShowOnceSettings(QObject* parent):
    base_type(kClientShowOnceId, base_type::StorageFormat::Section, parent)
{
}
