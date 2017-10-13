#include "system_health_list_model.h"
#include "private/system_health_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

SystemHealthListModel::SystemHealthListModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

SystemHealthListModel::~SystemHealthListModel()
{
}

} // namespace
} // namespace client
} // namespace nx
