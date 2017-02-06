#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>

class QnMobileClientUiController;

class QnLiteClientHandlerPrivate;

class QnLiteClientHandler: public Connective<QObject>
{
    Q_OBJECT

    using base_type = Connective<QObject>;

public:
    QnLiteClientHandler(QObject* parent = nullptr);
    ~QnLiteClientHandler();

    void setUiController(QnMobileClientUiController* controller);

private:
    Q_DECLARE_PRIVATE(QnLiteClientHandler)
    QScopedPointer<QnLiteClientHandlerPrivate> d_ptr;
};
