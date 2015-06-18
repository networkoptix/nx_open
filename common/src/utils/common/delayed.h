#pragma once

class QThread;

void executeDelayed(std::function<void()> callback, int delayMs = 1, QThread *targetThread = nullptr);
