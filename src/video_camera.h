#ifndef video_camera_h_2021
#define video_camera_h_2021

#include "camera/camera.h"

class VideoCamera : public CLVideoCamera
{
public:
	VideoCamera(CLDevice* device, CLVideoWindowItem* videovindow);

	~VideoCamera();
};

#endif //video_camera_h_2021
