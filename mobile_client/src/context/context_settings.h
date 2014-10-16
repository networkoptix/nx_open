#ifndef CONTEXT_SETTINGS_H
#define CONTEXT_SETTINGS_H

#include <QtCore/QObject>

class QnContextSettings : public QObject {
    Q_OBJECT
public:
    explicit QnContextSettings(QObject *parent = 0);

public slots:
    QVariantList savedSessions() const;
    void saveSession(const QVariantMap &session, int index);
};

#endif // CONTEXT_SETTINGS_H
