// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QString>
#include <QtCore/QVariantMap>

#include <nx/reflect/instrument.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API LicenseKey
{
    /**%apidoc License serial number. Corresponds to the License Block SERIAL property. */
    QnLatin1Array key;

    LicenseKey() = default;
    LicenseKey(const std::nullptr_t&) {}
    LicenseKey(const QString& id): key(id.toUtf8()) {}
    QString toString() const { return key; }
    const LicenseKey& getId() const { return *this; }

    bool operator==(const LicenseKey& other) const = default;
};
#define LicenseKey_Fields (key)
NX_VMS_API_DECLARE_STRUCT(LicenseKey)
NX_REFLECTION_INSTRUMENT(LicenseKey, LicenseKey_Fields)

struct NX_VMS_API LicenseData: LicenseKey
{
    /**%apidoc
     * License Block - a plain-text "property" file (with "key=value" lines). It is generated by
     * the License Server, is bound to the particular Server (via Hardware Id), and is signed
     * (the signature is stored in one of the properties). The License Block contains the following
     * properties (keys):
     * <ul>
     * <li><code>NAME</code> VMS product name.</li>
     * <li><code>SERIAL</code> Serial number of the License.</li>
     * <li><code>HWID</code> Hardware Id of the Server to which the License is bound.</li>
     * <li><code>COUNT</code> Number of cameras which are covered by the License.</li>
     * <li><code>CLASS</code> Type of the License. Defines which VMS platforms can use the License,
     *     and for what kind of camera-like devices.</li>
     * <li><code>VERSION</code> VMS version.</li>
     * <li><code>BRAND</code> VMS product name.</li>
     * <li><code>EXPIRATION</code> Expiration date in format <code>YYYY-MM-DD HH:MM:SS</code>.
     *     Optional; if omitted, the License lifetime is unlimited.</li>
     * <li><code>SIGNATURE2</code> Digital signature created by the License Server.</li>
     * <li><code>COMPANY</code> VMS vendor.</li>
     * </ul>
     * Additionally, other proprietary keys may appear in the License Block.
     * %// The following properties are intentionally left undocumented:
     *     - SUPPORT (missing in struct DetailedLicenseData).
     *     - DEACTIVATIONS (missing in struct DetailedLicenseData).
     *     - ORDERTYPE (used also to denote SAAS).
     */
    QnLatin1Array licenseBlock;

    bool operator==(const LicenseData& other) const = default;
    bool operator<(const LicenseData& other) const;
};
#define LicenseData_Fields (key)(licenseBlock)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(LicenseData)
NX_REFLECTION_INSTRUMENT(LicenseData, LicenseData_Fields)

/**
 * Structured representation of LicenseData::licenseBlock.
 */
struct NX_VMS_API DetailedLicenseData
{
    /**%apidoc Corresponds to the License Block property <code>SERIAL</code>. */
    QnLatin1Array key;

    /**%apidoc Corresponds to the License Block property <code>NAME</code>. */
    QString name;

    /**%apidoc Corresponds to the License Block property <code>COUNT</code>. */
    qint32 cameraCount = 0;

    /**%apidoc Corresponds to the License Block property <code>HWID</code>. */
    QString hardwareId;

    /**%apidoc Corresponds to the License Block property <code>CLASS</code>.
     * %// The values correspond to enum Qn::LicenseType; the correspondence is defined by the
     *     static array licenseTypeInfo in license.cpp. License type compatibility is defined by
     *     the static array compatibleLicenseType in license_usage_helper.cpp.
     */
    QString licenseType;

    /**%apidoc Corresponds to the License Block property <code>VERSION</code>. */
    QString version;

    /**%apidoc Corresponds to the License Block property <code>BRAND</code>. */
    QString brand;

    /**%apidoc Corresponds to the License Block property <code>EXPIRATION</code>. */
    QString expiration;

    /**%apidoc Corresponds to the License Block property <code>SIGNATURE2</code>. */
    QnLatin1Array signature;

    /**%apidoc[proprietary]*/
    QString orderType;

    /** Regional / License support data: company name. */
    QString company;

    /** Regional / License support data: contact address. */
    QString support;

    int deactivations = 0;

    bool operator==(const DetailedLicenseData& other) const = default;
};

struct NX_VMS_API LicenseSummaryData
{
    /**%apidoc Total license channels, including expired licenses. */
    int total = 0;

    /**%apidoc
     * Currently available license channels. It could be less than the total number of channels
     * in case of some licenses are not valid now (expired, server is offline e.t.c).
     */
    int available = 0;

    /**%apidoc Used license channels. */
    int inUse = 0;

    bool operator==(const LicenseSummaryData& other) const = default;

    QVariantMap toVariantMap() const
    {
        return
        {
            {"total", total},
            {"available", available},
            {"inUse", inUse}
        };
    }
};

struct NX_VMS_API LicenseSummaryDataEx: public LicenseSummaryData
{
    /**%apidoc
     * The list of exceed devices in case of total > available.
     */
    std::set<nx::Uuid> exceedDevices;
    nx::Uuid serviceId;
};

#define DetailedLicenseData_Fields \
    (key)(name)(cameraCount)(hardwareId)(licenseType)(version)(brand)(expiration)(signature)\
    (orderType)(company)(support)(deactivations)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(DetailedLicenseData)
NX_REFLECTION_INSTRUMENT(DetailedLicenseData, DetailedLicenseData_Fields)

#define LicenseSummaryData_Fields (total)(available)(inUse)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(LicenseSummaryData)
NX_REFLECTION_INSTRUMENT(LicenseSummaryData, LicenseSummaryData_Fields)

using LicenseSummaryValues = std::map<QString /*type*/, LicenseSummaryData /*summary*/>;

} // namespace api
} // namespace vms
} // namespace nx
