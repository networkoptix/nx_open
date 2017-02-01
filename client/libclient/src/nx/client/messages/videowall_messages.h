#pragma once

namespace nx {
namespace client {
namespace messages {

class VideoWall
{
    Q_DECLARE_TR_FUNCTIONS(VideoWall)
public:
    static void anotherVideoWall(QWidget* parent);

    static bool switchToVideoWallMode(QWidget* parent, bool* closeCurrentInstanse);
};


} // namespace messages
} // namespace client
} // namespace nx
