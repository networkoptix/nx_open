// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

class QSettings;

namespace nx {
namespace settings {

/** Generic class for persistent storage of 'once'-type flags. */
class NX_VMS_CLIENT_CORE_API ShowOnce: public QObject
{
    Q_OBJECT
    using base_type = QObject;
public:
    enum class StorageFormat
    {
        Section,
        File
    };

    ShowOnce(const QString& id,
        StorageFormat format = StorageFormat::Section,
        QObject* parent = nullptr);
    virtual ~ShowOnce();

    bool testFlag(const QString& key) const;
    void setFlag(const QString& key, bool value = true);

    void sync();
    void reset();

signals:
    void changed(const QString& key, bool value);

private:
    QScopedPointer<QSettings> m_storage;
};

} // namespace settings
} // namespace nx