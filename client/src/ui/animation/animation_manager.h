#ifndef animation_manager_h_2037
#define animation_manager_h_2037

class CLAbstractAnimation;

class CLAnimationManager : public QObject 
{
public:
	CLAnimationManager();
	~CLAnimationManager();

	void registerAnimation(CLAbstractAnimation* anim);
	void stopAllAnimations();
private:
	QList<CLAbstractAnimation*> m_anim_list;
};

#endif //animation_manager_h_2037
