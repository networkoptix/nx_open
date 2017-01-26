#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

class QSettings;

namespace nx {
namespace settings {

/** Generic class for persistent storage of 'once'-type flags. */
class ShowOnce: public QObject
{
    Q_OBJECT
    using base_type = QObject;
public:
    ShowOnce(const QString& id, QObject* parent = nullptr);
    virtual ~ShowOnce();

    bool testFlag(const QString& key) const;
    void setFlag(const QString& key, bool value = true);

    void sync();

signals:
    void changed(const QString& key, bool value);

private:
    QScopedPointer<QSettings> m_storage;
};

} // namespace settings
} // namespace nx