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

	// returns layout with recorders 
	LayoutContent* getAllRecordersContent();

	// generates layout for this recorder
	LayoutContent* createRecorderContent(QString id);


	// returns layout with recorders and top level custom layouts
	LayoutContent* getAllLayoutsContent();


	// creates new blank lay out content
	// client responsible for deleting this one 
	LayoutContent* getNewEmptyLayoutContent(unsigned int flags = 0);

	// returns empty content. does not create new one. this content used for layout editor as start one 
	LayoutContent* getEmptyLayoutContent();


private:
	
private:
	typedef QMap<QString, LayoutContent*> ContantsList;

	ContantsList mCustomContents;

	LayoutContent* mRootContent;
	LayoutContent* mAllRecorders;

	LayoutContent* mEmptyLayout;
	

};

#endif //layout_manager_h_2058