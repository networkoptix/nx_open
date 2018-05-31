#pragma once

#include <nx/utils/log/assert.h>

namespace nx {
namespace update {
namespace info {

struct NX_UPDATE_API OsVersion
{
    QString family;
    QString architecture;
    QString version;

    OsVersion(const QString& family, const QString& architecture, const QString& version);

    OsVersion() = default;
    OsVersion(const OsVersion&) = default;
    OsVersion& operator=(const OsVersion&) = default;

    bool isEmpty() const;
    bool matches(const QString& target) const;
    QString serialize() const;

    static OsVersion deserialize(const QString& s);
    static OsVersion fromString(const QString& s);
};

NX_UPDATE_API bool operator==(const OsVersion& lhs, const OsVersion& rhs);

NX_UPDATE_API OsVersion ubuntuX64();
NX_UPDATE_API OsVersion ubuntuX86();
NX_UPDATE_API OsVersion macosx();
NX_UPDATE_API OsVersion windowsX64();
NX_UPDATE_API OsVersion windowsX86();
NX_UPDATE_API OsVersion armBpi();
NX_UPDATE_API OsVersion armRpi();
NX_UPDATE_API OsVersion armBananapi();

} // namespace info
} // namespace update
} // namespace nx
