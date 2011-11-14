#ifndef QN_DISPLAY_STATE_H
#define QN_DISPLAY_STATE_H

#include <QObject>

class QnLayoutModel;
class QnLayoutGridMapper;
class QnLayoutItemModel;

class QnDisplayState: public QObject {
    Q_OBJECT;
public:
    enum Mode {
        VIEWING,
        EDITING
    };

    QnDisplayState(QnLayoutModel *model, QObject *parent = NULL);

    virtual ~QnDisplayState();

    QnLayoutModel *model() const {
        return m_model;
    }

    QnLayoutGridMapper *gridMapper() const {
        return m_gridMapper;
    }

    Mode mode() const {
        return m_mode;
    }
    
    void setMode(Mode mode);

    QnLayoutItemModel *selectedEntity() const {
        return m_selectedEntity;
    }

    void setSelectedEntity(QnLayoutItemModel *entity);

    QnLayoutItemModel *zoomedEntity() const {
        return m_zoomedEntity;
    }

    void setZoomedEntity(QnLayoutItemModel *entity);

signals:
    void modeChanged();
    void selectedEntityChanged(QnLayoutItemModel *oldSelectedEntity, QnLayoutItemModel *newSelectedEntity);
    void zoomedEntityChanged(QnLayoutItemModel *oldZoomedEntity, QnLayoutItemModel *newZoomedEntity);

private slots:
    void at_entity_aboutToBeRemoved(QnLayoutItemModel *entity);

private:
    QnLayoutModel *m_model;
    QnLayoutGridMapper *m_gridMapper;
    Mode m_mode;
    QnLayoutItemModel *m_selectedEntity;
    QnLayoutItemModel *m_zoomedEntity;
};

#endif // QN_DISPLAY_STATE_H
