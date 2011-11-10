#ifndef QN_DISPLAY_STATE_H
#define QN_DISPLAY_STATE_H

#include <QObject>

class QnDisplayModel;
class QnDisplayGridMapper;
class QnDisplayEntity;

class QnDisplayState: public QObject {
    Q_OBJECT;
public:
    enum Mode {
        VIEWING,
        EDITING
    };

    QnDisplayState(QnDisplayModel *model, QObject *parent = NULL);

    virtual ~QnDisplayState();

    QnDisplayModel *model() const {
        return m_model;
    }

    QnDisplayGridMapper *gridMapper() const {
        return m_gridMapper;
    }

    Mode mode() const {
        return m_mode;
    }
    
    void setMode(Mode mode);

    QnDisplayEntity *selectedEntity() const {
        return m_selectedEntity;
    }

    void setSelectedEntity(QnDisplayEntity *entity);

    QnDisplayEntity *zoomedEntity() const {
        return m_zoomedEntity;
    }

    void setZoomedEntity(QnDisplayEntity *entity);

signals:
    void modeChanged();
    void selectedEntityChanged(QnDisplayEntity *oldSelectedEntity, QnDisplayEntity *newSelectedEntity);
    void zoomedEntityChanged(QnDisplayEntity *oldZoomedEntity, QnDisplayEntity *newZoomedEntity);

private slots:
    void at_entity_aboutToBeRemoved(QnDisplayEntity *entity);

private:
    QnDisplayModel *m_model;
    QnDisplayGridMapper *m_gridMapper;
    Mode m_mode;
    QnDisplayEntity *m_selectedEntity;
    QnDisplayEntity *m_zoomedEntity;
};

#endif // QN_DISPLAY_STATE_H
