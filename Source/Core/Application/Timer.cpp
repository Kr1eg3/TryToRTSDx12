#include "Timer.h"
#include "../../Platform/Windows/WindowsPlatform.h"

// Static members
int64 Timer::s_frequency = 0;
bool Timer::s_frequencyInitialized = false;

Timer::Timer() {
    InitializeFrequency();
    Reset();
}

void Timer::Start() {
    int64 startTime = GetPerformanceCounter();

    if (m_stopped) {
        m_pausedTime += (startTime - m_stopTime);
        m_prevTime = startTime;
        m_stopTime = 0;
        m_stopped = false;
    }
}

void Timer::Stop() {
    if (!m_stopped) {
        int64 currTime = GetPerformanceCounter();
        m_stopTime = currTime;
        m_stopped = true;
    }
}

void Timer::Reset() {
    int64 currTime = GetPerformanceCounter();

    m_baseTime = currTime;
    m_prevTime = currTime;
    m_stopTime = 0;
    m_pausedTime = 0;
    m_stopped = false;
    m_frameCount = 0;
    m_fpsFrameCount = 0;
    m_fpsTimeElapsed = 0.0f;
}

void Timer::Tick() {
    if (m_stopped) {
        m_deltaTime = 0.0f;
        return;
    }

    int64 currTime = GetPerformanceCounter();
    m_currTime = currTime;

    // Calculate delta time
    m_deltaTime = (float32)(m_currTime - m_prevTime) / (float32)s_frequency;

    // Prevent negative delta time
    if (m_deltaTime < 0.0f) {
        m_deltaTime = 0.0f;
    }

    // Calculate total time
    m_totalTime = (float32)((m_currTime - m_pausedTime) - m_baseTime) / (float32)s_frequency;

    // Update frame count
    m_frameCount++;

    // Update FPS
    UpdateFPS();

    // Update previous time
    m_prevTime = m_currTime;
}

void Timer::UpdateFPS() {
    m_fpsFrameCount++;
    m_fpsTimeElapsed += m_deltaTime;

    // Update FPS every second
    if (m_fpsTimeElapsed >= 1.0f) {
        m_fps = (float32)m_fpsFrameCount / m_fpsTimeElapsed;
        m_fpsFrameCount = 0;
        m_fpsTimeElapsed = 0.0f;
    }
}

int64 Timer::GetPerformanceCounter() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

void Timer::InitializeFrequency() {
    if (!s_frequencyInitialized) {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        s_frequency = frequency.QuadPart;
        s_frequencyInitialized = true;
    }
}