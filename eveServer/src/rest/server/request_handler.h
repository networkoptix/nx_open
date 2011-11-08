#ifndef __REQUEST_HANDLER_H__
#define __REQUEST_HANDLER_H__

#include <QObject>
#include <QPair>
#include <QString>
#include <QList>
#include <QByteArray>
#include "utils/common/base.h"

typedef QList<QPair<QString, QString> > QnRequestParamList;

class QnRestRequestHandler: public QObject
{
public:
    virtual int executeGet(const QnRequestParamList& params, QByteArray& result) = 0;
    virtual int executePost(const QnRequestParamList& params, const QByteArray& body, QByteArray& result) = 0;
    virtual QString description() const { return QString(); }
};

class QnRestGUIRequestHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:
    QnRestGUIRequestHandler();
    virtual ~QnRestGUIRequestHandler();
    virtual int executeGet(const QnRequestParamList& params, QByteArray& result);
    virtual int executePost(const QnRequestParamList& params, const QByteArray& body, QByteArray& result);
protected:
    virtual int executeGetGUI(const QnRequestParamList& params, QByteArray& result) = 0;
    virtual int executePostGUI(const QnRequestParamList& params, const QByteArray& body, QByteArray& result) = 0;
private:
    Q_INVOKABLE void methodExecutor();
protected:
    QN_DECLARE_PRIVATE(QnRestGUIRequestHandler);
};

#endif // __REQUEST_HANDLER_H__
