
#pragma once

#include <nx/utils/singleton.h>
#include <network/abstract_systems_finder.h>

class QnSystemsFinder : public QnAbstractSystemsFinder
    , public Singleton<QnSystemsFinder>
{
    Q_OBJECT
    typedef QnAbstractSystemsFinder base_type;

public:
    QnSystemsFinder(QObject *parent = nullptr);

    virtual ~QnSystemsFinder();
    
public: //overrides
    SystemDescriptionList systems() const override;

private:
    typedef QScopedPointer<QnAbstractSystemsFinder> SystemsFinderPtr;

    const SystemsFinderPtr m_directSystemsFinder;
    const SystemsFinderPtr m_cloudSystemsFinder;
};


#define qnSystemsFinder QnSystemsFinder::instance()