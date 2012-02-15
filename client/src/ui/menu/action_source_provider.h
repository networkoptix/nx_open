#ifndef QN_ACTION_SOURCE_PROVIDER_H
#define QN_ACTION_SOURCE_PROVIDER_H

class QnAction;

class QnActionSourceProvider {
public:
    virtual ~QnActionSourceProvider() {};

    virtual QVariant source(QnAction *action) = 0;
};

#endif // QN_ACTION_SOURCE_PROVIDER_H
