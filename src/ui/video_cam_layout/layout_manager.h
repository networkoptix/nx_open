#ifndef layout_manager_h_2058
#define layout_manager_h_2058

#include <QString>
#include <QMap>

class LayoutContent;

class CLSceneLayoutManager
{
	CLSceneLayoutManager();
	bool load();
public:
	~CLSceneLayoutManager();
	static CLSceneLayoutManager& instance();

	void save();



	LayoutContent* startScreenLayoutContent();
	bool hasCustomLayoutsContent() const;
	LayoutContent* getDefaultLayoutContent();

	void addRecorderLayoutContent(QString id);
	LayoutContent* getRecorderLyoutContent(QString id);
	LayoutContent* getAllLayoutsContent();

private:
	typedef QMap<QString, LayoutContent*> ContantsList;

	ContantsList mRecordersContents;
	ContantsList mCustomContents;

	LayoutContent* mRootContent;
	

};

#endif //layout_manager_h_2058