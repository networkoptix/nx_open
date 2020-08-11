#pragma once

#include <set>
#include <memory>

class QnAbstractDataReceptor;

namespace nx::vms::server::analytics {

using MetadataSinkPtr = std::shared_ptr<QnAbstractDataReceptor>;
using MetadataSinkSet = std::set<MetadataSinkPtr>;

} // namespace nx::vms::server::analytics
