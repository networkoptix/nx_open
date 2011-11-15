#ifndef QN_WORKBENCH_H
#define QN_WORKBENCH_H

#include <QObject>

class QnWorkbenchLayout;
class QnWorkbenchGridMapper;
class QnWorkbenchItem;

class QnWorkbench: public QObject {
    Q_OBJECT;
public:
    enum Mode {
        VIEWING,
        EDITING
    };

    QnWorkbench(QObject *parent = NULL);

    virtual ~QnWorkbench();

    QnWorkbenchLayout *layout() const {
        return m_layout;
    }

    void setLayout(QnWorkbenchLayout *layout);

    QnWorkbenchGridMapper *mapper() const {
        return m_mapper;
    }

    Mode mode() const {
        return m_mode;
    }
    
    void setMode(Mode mode);

    QnWorkbenchItem *selectedItem() const {
        return m_selectedItem;
    }

    void setSelectedItem(QnWorkbenchItem *item);

    QnWorkbenchItem *zoomedItem() const {
        return m_zoomedItem;
    }

    void setZoomedItem(QnWorkbenchItem *item);

signals:
    void aboutToBeDestroyed();
    void modeChanged();
    void layoutChanged(QnWorkbenchLayout *oldLayout, QnWorkbenchLayout *newLayout);
    void selectedItemChanged(QnWorkbenchItem *oldSelectedEntity, QnWorkbenchItem *newSelectedEntity);
    void zoomedItemChanged(QnWorkbenchItem *oldZoomedEntity, QnWorkbenchItem *newZoomedEntity);

private slots:
    void at_item_aboutToBeRemoved(QnWorkbenchItem *item);
    void at_layout_aboutToBeDestroyed();

private:
    QnWorkbenchLayout *m_layout;
    QnWorkbenchGridMapper *m_mapper;
    Mode m_mode;
    QnWorkbenchItem *m_selectedItem;
    QnWorkbenchItem *m_zoomedItem;
};

#endif // QN_WORKBENCH_H
