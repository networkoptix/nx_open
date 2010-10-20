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


	// start screen 
	LayoutContent* startScreenLayoutContent();

	// return true if we have at least one custome layout
	bool hasCustomLayoutsContent() const;

	// if we have custom layouts and one of them default
	LayoutContent* getDefaultLayoutContent();

	void addRecorderLayoutContent(QString id);

	// returns layout with recorders only
	LayoutContent* getRecorderLyoutContent(QString id);

	// returns layout with recorders and top level custom layouts
	LayoutContent* getAllLayoutsContent();

private:
	typedef QMap<QString, LayoutContent*> ContantsList;

	ContantsList mRecordersContents;
	ContantsList mCustomContents;

	LayoutContent* mRootContent;
	

};

#endif //layout_manager_h_2058