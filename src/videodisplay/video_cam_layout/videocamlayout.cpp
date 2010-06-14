#include "./videocamlayout.h"
#include "../../camera/camera.h"
#include "../../device/device_video_layout.h"
#include "../../device/device.h"

VideoCamerasLayout::VideoCamerasLayout(unsigned int max_rows, unsigned int max_slots)
{
	m_slots = max_slots/max_rows*max_rows;// integer number of rows
	m_height = max_rows;
	m_width = m_slots/m_height;
	m_cams = new CLVideoCamera*[m_slots],
	m_potential_energy = new unsigned long[m_slots];


	memset(m_cams, 0, m_slots*sizeof(CLVideoCamera*));

	for (int i = 0; i <m_slots;++i)
	{
		int x = i%m_width;
		int y = i/m_width;

		//m_potential_energy[i] = x*x + y*(y+1);  // y+1 is coz with all other equals we want to extend scene horizontaly
		m_potential_energy[i] = x*x + y*y*4/3;  // y+1 is coz with all other equals we want to extend scene horizontaly

	}
}

VideoCamerasLayout::~VideoCamerasLayout()
{
	delete[] m_cams;
	delete[] m_potential_energy;
}

int VideoCamerasLayout::getNextAvailablePos(const CLDeviceVideoLayout* layout) const
{
	unsigned long min_energy = 0xffffffff; // very beig value
	int min_position = -1;

	int width = layout->width();
	int height = layout->height();

	int candidate = 0;

	while( (candidate = getNextAvailablePosWidthHeight(candidate, width, height,  min_energy))>=0 )
	{
		min_position = candidate;
		candidate++;
	}


	return min_position;
}

void VideoCamerasLayout::getXY(unsigned int pos, int& x, int& y) const
{
	x = pos%m_width;
	y = pos/m_width;
}

int VideoCamerasLayout::getPos(const CLVideoCamera* cam) const
{
	for (int i = 0; i <m_slots; ++i)
		if (m_cams[i] == cam)
		{
			return i;
		}

	return -1;
}

bool VideoCamerasLayout::getXY(const CLVideoCamera* cam, int& x, int& y) const
{
	int pos = getPos(cam);
	if (pos<0)
		return false;

	getXY(pos, x, y);
	return true;
}

void VideoCamerasLayout::addCamera(int position, CLVideoCamera* cam)
{
	int width = cam->getDevice()->getVideoLayout()->width();
	int height = cam->getDevice()->getVideoLayout()->height();

	for(int dy = 0; dy < height; ++dy)
	{
		for(int dx = 0; dx < width; ++dx)
		{
			int pos = position + dx + m_width*dy;
			m_cams[pos] = cam;
		}
	}

	
}

void VideoCamerasLayout::removeCamera(CLVideoCamera* cam)
{
	for (int i = 0; i <m_slots; ++i)
		if (m_cams[i] == cam)
			m_cams[i] = 0;
}

CLVideoCamera* VideoCamerasLayout::getNextLeftCam(const CLVideoCamera* curr) const
{
	int original_pos = getPos(curr);
	if (original_pos<0)
		return 0;

	int pos = original_pos;

	while(1)
	{
		--pos;
		if (pos<0)
			pos = m_slots-1;

		if (m_cams[pos]!=0 && m_cams[pos]!=curr)
			return m_cams[pos];

		if (pos == original_pos)
			return 0;
	}

}

CLVideoCamera* VideoCamerasLayout::getNextRightCam(const CLVideoCamera* curr) const
{
	int original_pos = getPos(curr);
	if (original_pos<0)
		return 0;

	int pos = original_pos;

	while(1)
	{
		++pos;
		if (pos>m_slots-1)
			pos = 0;

		if (m_cams[pos]!=0 && m_cams[pos]!=curr)
			return m_cams[pos];

		if (pos == original_pos)
			return 0;
	}

}

CLVideoCamera* VideoCamerasLayout::getNextTopCam(const CLVideoCamera* curr) const
{
	int original_pos = getPos(curr);
	if (original_pos<0)
		return 0;

	int pos = original_pos;

	while(1)
	{
		if (pos==0)
			pos = m_slots - 1;
		else
			pos-=m_width;


		if (pos<0)
			pos += (m_width*m_height - 1);

		if (m_cams[pos]!=0 && m_cams[pos]!=curr)
			return m_cams[pos];

		if (pos == original_pos)
			return 0;
	}

}

CLVideoCamera* VideoCamerasLayout::getNextBottomCam(const CLVideoCamera* curr) const
{
	int original_pos = getPos(curr);
	if (original_pos<0)
		return 0;

	int pos = original_pos;

	while(1)
	{
		if (pos == m_slots-1 ) // very last one 
			pos = 0;
		else
			pos+=m_width;

	
		if (pos>m_slots-1)
			pos -= (m_width*m_height - 1);

		if (m_cams[pos]!=0 && m_cams[pos]!=curr)
			return m_cams[pos];

		if (pos == original_pos)
			return 0;
	}

}


//==========================================================================================
int VideoCamerasLayout::getNextAvalableSlot(unsigned int start_point, unsigned long min_energy)const
{
	if (start_point>=m_slots)
		return -1;

	for (int i = start_point; i <m_slots; ++i)
		if (m_cams[i] == 0 && m_potential_energy[i] <= min_energy)
			return i;

	return -1;
}

int VideoCamerasLayout::getNextAvailablePosWidthHeight(unsigned int start_point, int width, int height, unsigned long& min_energy) const
{
	int candidate = 0;

	while( (candidate=getNextAvalableSlot(start_point, min_energy))>=0)
	{
		++start_point;

		int x = candidate%m_width;
		int y = candidate/m_width;

		if ( x+width > m_width)
			continue;

		if ( y+height> m_height)
			continue;

		bool need_continue = false;
		int energy = 0;

		for(int dy = 0; dy < height; ++dy)
		{
			for(int dx = 0; dx < width; ++dx)
			{
				int pos = candidate + dx + m_width*dy;
				
				if (m_cams[pos]!=0)
				{
					need_continue = true;
					break;
				}

				energy+=m_potential_energy[pos];

				if (energy>=min_energy)
				{
					need_continue = true;
					break;
				}


			}

			if (need_continue)
				break;
		}

		if (need_continue)
			continue;

		min_energy = energy;
		return candidate;

	}

	return -1;
}

