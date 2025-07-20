#pragma once

#include "../../Core/Utilities/Types.h"

class Timer {
public:
    Timer();

    // Timer operations
    void Start();
    void Stop();
    void Reset();
    void Tick();

    // Time queries
    float32 GetDeltaTime() const { return m_deltaTime; }
    float32 GetTotalTime() const { return m_totalTime; }
    uint64 GetFrameCount() const { return m_frameCount; }
    float32 GetFPS() const { return m_fps; }

    // State queries
    bool IsRunning() const { return !m_stopped; }
    bool IsStopped() const { return m_stopped; }

private:
    void UpdateFPS();

private:
    // Time tracking
    int64 m_baseTime = 0;           // Base time when timer was reset
    int64 m_pausedTime = 0;         // Total time paused
    int64 m_stopTime = 0;           // Time when timer was stopped
    int64 m_prevTime = 0;           // Previous frame time
    int64 m_currTime = 0;           // Current frame time

    // Calculated values
    float32 m_deltaTime = 0.0f;     // Time between frames
    float32 m_totalTime = 0.0f;     // Total running time
    uint64 m_frameCount = 0;        // Total frame count

    // FPS calculation
    float32 m_fps = 0.0f;
    uint32 m_fpsFrameCount = 0;
    float32 m_fpsTimeElapsed = 0.0f;

    // Timer state
    bool m_stopped = false;

    // Performance counter frequency
    static int64 s_frequency;
    static bool s_frequencyInitialized;

    // Helper methods
    static int64 GetPerformanceCounter();
    static void InitializeFrequency();
};