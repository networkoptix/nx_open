#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/streaming/abstract_stream_data_provider.h>

class QnDataProviderFactory: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit QnDataProviderFactory(QObject* parent = nullptr);
    virtual ~QnDataProviderFactory() override;

    virtual QnAbstractStreamDataProvider* createDataProvider(
        const QnResourcePtr& resource,
        Qn::ConnectionRole role = Qn::CR_Default);

    using DataProviderGenerator = std::function<QnAbstractStreamDataProvider*(
        const QnResourcePtr&, Qn::ConnectionRole)>;

    template<class T>
    void registerResourceType()
    {
        registerResourceType(T::staticMetaObject, T::createDataProvider);
    }

    void registerResourceType(
        const QMetaObject& metaobject,
        DataProviderGenerator&& generator);
private:

    struct Private;
    QScopedPointer<Private> d;
};
