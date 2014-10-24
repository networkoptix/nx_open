#ifndef QN_SPACE_MAPPER_H
#define QN_SPACE_MAPPER_H

#include <boost/array.hpp>

#include <QtCore/QVariant>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>

#include "interpolator.h"


template<class T>
class QnSpaceMapper {
public:
    virtual ~QnSpaceMapper() {}

    virtual T sourceToTarget(const T &source) const = 0;
    virtual T targetToSource(const T &target) const = 0;
};


template<class T>
class QnSpaceMapperPtr: public QSharedPointer<QnSpaceMapper<T> > {
    typedef QSharedPointer<QnSpaceMapper<T> > base_type;
public:
    QnSpaceMapperPtr() {}
    QnSpaceMapperPtr(QnSpaceMapper<T> *mapper): base_type(mapper) {}
};


template<class T>
class QnIdentitySpaceMapper: public QnSpaceMapper<T> {
public:
    virtual T sourceToTarget(const T &source) const override { return source; }
    virtual T targetToSource(const T &target) const override { return target; }
};


template<class T>
class QnScalarInterpolationSpaceMapper: public QnSpaceMapper<T> {
public:
    QnScalarInterpolationSpaceMapper(qreal source0, qreal source1, qreal target0, qreal target1, Qn::ExtrapolationMode extrapolationMode) {
        QVector<QPair<qreal, qreal> > sourceToTarget;
        sourceToTarget.push_back(qMakePair(source0, target0));
        sourceToTarget.push_back(qMakePair(source1, target1));
        init(sourceToTarget, extrapolationMode);
    }

    QnScalarInterpolationSpaceMapper(const QVector<QPair<qreal, qreal> > &sourceToTarget, Qn::ExtrapolationMode extrapolationMode) {
        init(sourceToTarget, extrapolationMode);
    }

    virtual T sourceToTarget(const T &source) const override { return m_sourceToTarget(source); }
    virtual T targetToSource(const T &target) const override { return m_targetToSource(target); }

private:
    void init(const QVector<QPair<qreal, qreal> > &sourceToTarget, Qn::ExtrapolationMode extrapolationMode) {
        QVector<QPair<qreal, qreal> > targetToSource;
        for(const auto &point: sourceToTarget)
            targetToSource.push_back(qMakePair(point.second, point.first));

        m_sourceToTarget.setPoints(sourceToTarget);
        m_sourceToTarget.setExtrapolationMode(extrapolationMode);
        m_targetToSource.setPoints(targetToSource);
        m_targetToSource.setExtrapolationMode(extrapolationMode);
    }

private:
    QnInterpolator<T> m_sourceToTarget, m_targetToSource;
};


class QnSeparableVectorSpaceMapper: public QnSpaceMapper<QVector3D> {
public:
    QnSeparableVectorSpaceMapper(const QnSpaceMapperPtr<qreal> &xMapper, const QnSpaceMapperPtr<qreal> &yMapper, const QnSpaceMapperPtr<qreal> &zMapper) {
        m_mappers[0] = xMapper;
        m_mappers[1] = yMapper;
        m_mappers[2] = zMapper;
    }

    QVector3D sourceToTarget(const QVector3D &source) const { 
        return QVector3D(
            m_mappers[0]->sourceToTarget(source.x()),
            m_mappers[1]->sourceToTarget(source.y()),
            m_mappers[2]->sourceToTarget(source.z())
        );
    }

    QVector3D targetToSource(const QVector3D &target) const { 
        return QVector3D(
            m_mappers[0]->targetToSource(target.x()),
            m_mappers[1]->targetToSource(target.y()),
            m_mappers[2]->targetToSource(target.z())
        );
    }

private:
    boost::array<QnSpaceMapperPtr<qreal>, 3> m_mappers;
};


#endif // QN_SPACE_MAPPER_H
