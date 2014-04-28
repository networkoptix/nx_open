#ifndef SYSTEM_INFORMATION_H
#define SYSTEM_INFORMATION_H

#include <QtCore/QString>
#include <QtCore/QRegExp>
#include <QtCore/QMetaType>

#include <boost/operators.hpp>

#include <nx_ec/binary_serialization_helper.h>

class QnSystemInformation : public boost::equality_comparable1<QnSystemInformation> {
public:
    QnSystemInformation(const QString &platform, const QString &arch) :
        arch(arch),
        platform(platform)
    {}

    QnSystemInformation(const QString &infoString) {
        QRegExp infoRegExp(lit("(.+) (.+)"));
        QnSystemInformation info;
        if (infoRegExp.exactMatch(infoString)) {
            platform = infoRegExp.cap(1);
            arch = infoRegExp.cap(2);
        }
    }

    QnSystemInformation() {}

    QString toString() const {
        return platform + lit(" ") + arch;
    }

    bool isValid() const {
        return !platform.isEmpty() && !arch.isEmpty();
    }

    bool operator==(const QnSystemInformation &other) const {
        return arch == other.arch && platform == other.platform;
    }

    QString arch;
    QString platform;
};

inline uint qHash(const QnSystemInformation &sysInfo) {
    return qHash(sysInfo.toString());
}

Q_DECLARE_METATYPE(QnSystemInformation)
QN_DEFINE_STRUCT_SERIALIZATORS(QnSystemInformation, (arch) (platform))

#endif // SYSTEM_INFORMATION_H
