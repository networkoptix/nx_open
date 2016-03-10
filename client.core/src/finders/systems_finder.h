
#pragma once

#include <nx/utils/singleton.h>
#include <utils/common/connections_holder.h>
#include <finders/abstract_systems_finder.h>

class ConnectionsHolder;

class QnSystemsFinder : public QnAbstractSystemsFinder
    , public Singleton<QnSystemsFinder>
{
    Q_OBJECT
    typedef QnAbstractSystemsFinder base_type;

public:
    explicit QnSystemsFinder(QObject *parent = nullptr);

    virtual ~QnSystemsFinder();
    
    void addSystemsFinder(QnAbstractSystemsFinder *finder);

public: //overrides
    SystemDescriptionList systems() const override;

private:
    typedef QMap<QnAbstractSystemsFinder *
        , QnDisconnectHelperPtr> SystemsFinderList;

    SystemsFinderList m_finders;
};


#define qnSystemsFinder QnSystemsFinder::instance()