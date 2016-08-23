
#pragma once

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

    void setScene(QGraphicsScene* scene);

    qreal scale() const;

signals:
    void scaleChanged();

private:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

    void updateScale();

private:
    using ViewportPointer = QPointer<QGraphicsView>;

    ViewportPointer m_viewport;
    QTransform m_transform;
    qreal m_scale;
};