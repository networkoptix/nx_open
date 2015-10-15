#pragma once

#include <QtCore/QObject>

class QnClientModule;
class QnSkin;
class QnCustomizer;

class QnAxClientModule: public QObject {
    Q_OBJECT
public:
    QnAxClientModule(QObject *parent = nullptr);
    virtual ~QnAxClientModule();

private:
    QScopedPointer<QnClientModule> m_clientModule;
    QScopedPointer<QnSkin> m_skin;
    QScopedPointer<QnCustomizer> m_customizer;
};