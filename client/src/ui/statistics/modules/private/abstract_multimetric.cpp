
#include "abstract_multimetric.h"

AbstractMultimetric::AbstractMultimetric()
{}

AbstractMultimetric::~AbstractMultimetric()
{}

QnMetricsHash AbstractMultimetric::metrics() const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
    return QnMetricsHash();
}
