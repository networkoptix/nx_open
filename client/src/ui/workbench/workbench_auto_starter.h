#ifndef QN_WORKBENCH_AUTO_STARTER_H
#define QN_WORKBENCH_AUTO_STARTER_H

#include <QObject>

#include "workbench_context_aware.h"

class QnWorkbenchAutoStarter: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchAutoStarter(QObject *parent = NULL);
    virtual ~QnWorkbenchAutoStarter();

    bool isSupported() const;

    bool isAutoStartEnabled();
    void setAutoStartEnabled(bool autoStartEnabled);

protected:
    virtual int settingsKey() const;
    virtual QString autoStartPath() const;
    virtual QString autoStartKey() const;

    QString fromRegistryFormat(const QString &path) const;
    QString toRegistryFormat(const QString &path) const;

private:
    Q_SLOT void at_autoStartSetting_valueChanged();
};


#endif // QN_WORKBENCH_AUTO_STARTER_H
