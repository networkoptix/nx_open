#ifndef layout_manager_h_2058
#define layout_manager_h_2058

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

	// if we have custom layouts and one of them default
	LayoutContent* getDefaultLayoutContent();

	// if new recorder in the system, this functioun should be called 
	void addRecorderLayoutContent(QString id);

	// generates layout for this recorder
	LayoutContent* createRecorderContent(QString id);

	// returns layout with recorders 
	LayoutContent* getAllRecordersContent();

	// returns layout with recorders and top level custom layouts
	LayoutContent* getAllLayoutsContent();

	// old one will be deleted 
	// such layout mostly will be used for layout editor
	LayoutContent* generateAllRecordersAndLayouts();

	// creates new blank lay out content
	// client responsible for deleting this one 
	LayoutContent* getNewEmptyLayoutContent(unsigned int flags = 0);

	// returns empty content. does not create new one. this content used for layout editor as start one 
	LayoutContent* getEmptyLayoutContent();

	LayoutContent* getSearchLayout();

private:
	bool load_parseLyout(const QDomElement& layout, LayoutContent* parent);
private:
	LayoutContent* mAllCustomLayouts;
	LayoutContent* mAllRecorders;

	LayoutContent* mEmptyLayout;
	LayoutContent* mSearchLayout;

	LayoutContent* mRecordersAndLayouts;

};

#endif //layout_manager_h_2058