#ifndef QN_MEDIA_SERVER_MODULE_H
#define QN_MEDIA_SERVER_MODULE_H

#include <QtCore/QObject>

class QnCommonModule;

class QnMediaServerModule: public QObject {
    Q_OBJECT;
public:
    QnMediaServerModule(int &argc, char **argv, QObject *parent = NULL);
    virtual ~QnMediaServerModule();

protected:
    void loadPtzMappers(const QString &fileName);

private:
    QnCommonModule *m_common;
};



#endif // QN_MEDIA_SERVER_MODULE_H
