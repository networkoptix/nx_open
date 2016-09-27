
#pragma once

#include <QtCore/QObject>

class QTimer;

class QnSystemsWeightUpadater : public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnSystemsWeightUpadater(QObject* parent = nullptr);

    virtual ~QnSystemsWeightUpadater() = default;

private:
    void updateWeights();

private:
    QTimer* m_updateTimer;
};