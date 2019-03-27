#pragma once

class MediaServerLauncher;

namespace nx::test_support {

void addTestStorage(MediaServerLauncher* server, const QString& path);

} // namespace nx::test_support