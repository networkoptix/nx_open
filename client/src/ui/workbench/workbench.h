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

    QnDisplayState(QObject *parent = NULL);

    virtual ~QnDisplayState();

    QnLayoutModel *layout() const {
        return m_layout;
    }

    void setLayout(QnLayoutModel *layout);

    QnLayoutGridMapper *mapper() const {
        return m_mapper;
    }

    Mode mode() const {
        return m_mode;
    }
    
    void setMode(Mode mode);

    QnLayoutItemModel *selectedItem() const {
        return m_selectedItem;
    }

    void setSelectedItem(QnLayoutItemModel *item);

    QnLayoutItemModel *zoomedItem() const {
        return m_zoomedItem;
    }

    void setZoomedItem(QnLayoutItemModel *item);

signals:
    void aboutToBeDestroyed();
    void modeChanged();
    void layoutChanged(QnLayoutModel *oldLayout, QnLayoutModel *newLayout);
    void selectedItemChanged(QnLayoutItemModel *oldSelectedEntity, QnLayoutItemModel *newSelectedEntity);
    void zoomedItemChanged(QnLayoutItemModel *oldZoomedEntity, QnLayoutItemModel *newZoomedEntity);

private slots:
    void at_item_aboutToBeRemoved(QnLayoutItemModel *item);
    void at_layout_aboutToBeDestroyed();

private:
    QnLayoutModel *m_layout;
    QnLayoutGridMapper *m_mapper;
    Mode m_mode;
    QnLayoutItemModel *m_selectedItem;
    QnLayoutItemModel *m_zoomedItem;
};

#endif // QN_DISPLAY_STATE_H
