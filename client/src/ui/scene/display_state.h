#ifndef QN_DISPLAY_STATE_H
#define QN_DISPLAY_STATE_H

#include <QObject>

class QnUiLayout;
class QnDisplayGridMapper;
class QnUiLayoutItem;

class QnDisplayState: public QObject {
    Q_OBJECT;
public:
    enum Mode {
        VIEWING,
        EDITING
    };

    QnDisplayState(QnUiLayout *model, QObject *parent = NULL);

    virtual ~QnDisplayState();

    QnUiLayout *model() const {
        return m_model;
    }

    QnDisplayGridMapper *gridMapper() const {
        return m_gridMapper;
    }

    Mode mode() const {
        return m_mode;
    }
    
    void setMode(Mode mode);

    QnUiLayoutItem *selectedEntity() const {
        return m_selectedEntity;
    }

    void setSelectedEntity(QnUiLayoutItem *entity);

    QnUiLayoutItem *zoomedEntity() const {
        return m_zoomedEntity;
    }

    void setZoomedEntity(QnUiLayoutItem *entity);

signals:
    void modeChanged();
    void selectedEntityChanged(QnUiLayoutItem *oldSelectedEntity, QnUiLayoutItem *newSelectedEntity);
    void zoomedEntityChanged(QnUiLayoutItem *oldZoomedEntity, QnUiLayoutItem *newZoomedEntity);

private slots:
    void at_entity_aboutToBeRemoved(QnUiLayoutItem *entity);

private:
    QnUiLayout *m_model;
    QnDisplayGridMapper *m_gridMapper;
    Mode m_mode;
    QnUiLayoutItem *m_selectedEntity;
    QnUiLayoutItem *m_zoomedEntity;
};

#endif // QN_DISPLAY_STATE_H
