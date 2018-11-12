#pragma once

#include <QtCore/QObject>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/software_version.h>

namespace nx {
namespace vms {
namespace api {

class NX_VMS_API SoftwareVersion: public nx::utils::SoftwareVersion
{
    Q_GADGET
    Q_PROPERTY(int major READ major CONSTANT)
    Q_PROPERTY(int minor READ minor CONSTANT)
    Q_PROPERTY(int bugfix READ bugfix CONSTANT)
    Q_PROPERTY(int build READ build CONSTANT)

    using base_type = nx::utils::SoftwareVersion;

public:
    SoftwareVersion() = default;
    SoftwareVersion(const base_type& other): base_type(other) {}

    using base_type::base_type; //< Forward other constructors.

    bool isCompatible(const SoftwareVersion &r) const;
    friend bool isCompatible(const SoftwareVersion& l, const SoftwareVersion& r)
    {
        return l.isCompatible(r);
    }

    Q_INVOKABLE QString toString(Format format = FullFormat) const
    {
        return base_type::toString(format);
    }

    Q_INVOKABLE bool isNull() const { return base_type::isNull(); }

    Q_INVOKABLE bool isLessThan(const SoftwareVersion& other) const { return *this < other; }

    QN_FUSION_DECLARE_FUNCTIONS(SoftwareVersion,
        (ubjson)(json)(xml)(lexical)(datastream)(csv_field),
        friend NX_VMS_API)
};

} // namespace api
} // namespace vms
} // namespace nx

uint qHash(const nx::vms::api::SoftwareVersion& softwareVersion);

Q_DECLARE_METATYPE(nx::vms::api::SoftwareVersion)
