#include "abstract_animation.h"

CLAbstractAnimation::CLAbstractAnimation():
m_deleteAfterFinished(false)
{

}

CLAbstractAnimation::~CLAbstractAnimation()
{

}

void CLAbstractAnimation::setDeleteAfterFinished(bool val)
{
	m_deleteAfterFinished = val;
}


bool CLAbstractAnimation::deleteAfterFinished() const
{
	return m_deleteAfterFinished;
}