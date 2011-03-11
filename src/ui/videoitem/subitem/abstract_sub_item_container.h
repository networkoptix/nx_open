#ifndef abstract_sub_item_container_h1031
#define abstract_sub_item_container_h1031

class CLAbstractSubItem;

enum CLSubItemType {CloseSubItem, ArchiveNavigatorSubItem, RecordingSubItem, PlaySubItem, PauseSubItem, StepForwardSubItem, StepBackwardSubItem, RewindBackwardSubItem, RewindForwardSubItem};

class CLAbstractSubItemContainer : public QObject, public QGraphicsItem
{
	Q_OBJECT
public:
	CLAbstractSubItemContainer(QGraphicsItem* parent);

	~CLAbstractSubItemContainer();

    QList<CLAbstractSubItem*> subItemList() const;

    void addSubItem(CLAbstractSubItem *item);
    void removeSubItem(CLAbstractSubItem *item);

	bool addSubItem(CLSubItemType type);
	virtual void removeSubItem(CLSubItemType type);
	virtual QPointF getBestSubItemPos(CLSubItemType type);

	void addToEevntTransparetList(QGraphicsItem* item);
	bool isInEventTransparetList(QGraphicsItem* item) const;

signals:
	void onClose(CLAbstractSubItemContainer*);

public slots:
	void onResize();

protected slots:
	virtual void onSubItemPressed(CLAbstractSubItem* subitem);

protected:

	QList<QGraphicsItem*> m_eventtransperent_list;
    QList<CLAbstractSubItem*> m_subItems;
};

#endif //abstract_sub_item_container_h1031