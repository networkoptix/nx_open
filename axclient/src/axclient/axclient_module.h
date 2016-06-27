#pragma once

#include <QtCore/QObject>

class QnClientModule;

class QnAxClientModule: public QObject {
    Q_OBJECT
public:
    QnAxClientModule(QObject *parent = nullptr);
    virtual ~QnAxClientModule();

private:
    QScopedPointer<QnClientModule> m_clientModule;
};