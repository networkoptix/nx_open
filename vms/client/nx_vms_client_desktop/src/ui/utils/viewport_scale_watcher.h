// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtGui/QTransform>

class QGraphicsScene;
class QGraphicsView;

class QnViewportScaleWatcher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal scale READ scale NOTIFY scaleChanged)

    using base_type = QObject;
public:
    QnViewportScaleWatcher(QObject* parent = nullptr);

    ~QnViewportScaleWatcher();

    bool initialized() const;

    void initialize(QGraphicsScene* scene);

    qreal scale() const;

public:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void scaleChanged(qreal value);

private:
    void updateScale();

private:
    using ViewportPointer = QPointer<QGraphicsView>;

    ViewportPointer m_viewport;
    QTransform m_transform;
    qreal m_scale;
};
