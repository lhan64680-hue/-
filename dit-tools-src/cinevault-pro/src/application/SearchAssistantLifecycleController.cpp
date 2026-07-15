#include "application/SearchAssistantLifecycleController.h"

#include "infrastructure/config/AppSettings.h"
#include "infrastructure/search/LocalSearchAssistantRuntime.h"

namespace {
constexpr int kStartupPreloadDelayMs = 250;
constexpr int kMillisecondsPerMinute = 60 * 1000;
}

SearchAssistantLifecycleController::SearchAssistantLifecycleController(
    AppSettings *settings,
    LocalSearchAssistantRuntime *runtime,
    QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_runtime(runtime)
    , m_idleMonitor(this)
{
    m_preloadTimer.setSingleShot(true);
    connect(&m_preloadTimer,
            &QTimer::timeout,
            this,
            &SearchAssistantLifecycleController::preload);
    connect(&m_idleMonitor,
            &ApplicationIdleMonitor::becameIdle,
            this,
            &SearchAssistantLifecycleController::unloadForIdle);
    connect(&m_idleMonitor,
            &ApplicationIdleMonitor::activityResumed,
            this,
            &SearchAssistantLifecycleController::resumeFromIdle);
    if (m_runtime) {
        connect(m_runtime,
                &LocalSearchAssistantRuntime::statusChanged,
                this,
                &SearchAssistantLifecycleController::lifecycleChanged);
    }
}

void SearchAssistantLifecycleController::start()
{
    if (m_started) {
        applySettings();
        return;
    }
    m_started = true;
    applySettings();
}

void SearchAssistantLifecycleController::applySettings()
{
    if (!m_started || !m_settings || !m_runtime) {
        return;
    }

    m_preloadTimer.stop();
    if (!m_settings->searchAssistantEnabled()) {
        m_idleMonitor.stop();
        m_idleUnloaded = false;
        m_runtime->stop();
        emit lifecycleChanged();
        return;
    }

    m_idleMonitor.start(autoUnloadMinutes() * kMillisecondsPerMinute);
    if (m_runtime->isReady() || m_runtime->isStarting()) {
        m_idleUnloaded = false;
        emit lifecycleChanged();
        return;
    }

    m_idleUnloaded = false;
    schedulePreload(kStartupPreloadDelayMs);
    emit lifecycleChanged();
}

void SearchAssistantLifecycleController::recordUserActivity()
{
    m_idleMonitor.recordActivity();
}

bool SearchAssistantLifecycleController::isStarted() const
{
    return m_started;
}

bool SearchAssistantLifecycleController::isIdleUnloaded() const
{
    return m_idleUnloaded;
}

int SearchAssistantLifecycleController::autoUnloadMinutes() const
{
    return m_settings ? m_settings->searchAssistantAutoUnloadMinutes() : 60;
}

void SearchAssistantLifecycleController::schedulePreload(int delayMs)
{
    if (!m_started || !m_settings || !m_settings->searchAssistantEnabled()) {
        return;
    }
    m_preloadTimer.start(qMax(0, delayMs));
}

void SearchAssistantLifecycleController::preload()
{
    if (!m_started || !m_settings || !m_runtime
        || !m_settings->searchAssistantEnabled()) {
        return;
    }
    m_idleUnloaded = false;
    m_runtime->start();
    emit lifecycleChanged();
}

void SearchAssistantLifecycleController::unloadForIdle()
{
    if (!m_started || !m_settings || !m_runtime
        || !m_settings->searchAssistantEnabled()) {
        return;
    }

    m_preloadTimer.stop();
    if (m_runtime->isReady() || m_runtime->isStarting()) {
        m_runtime->stop();
        m_idleUnloaded = true;
        emit lifecycleChanged();
    }
}

void SearchAssistantLifecycleController::resumeFromIdle()
{
    if (!m_idleUnloaded || !m_started || !m_settings
        || !m_settings->searchAssistantEnabled()) {
        return;
    }
    m_idleUnloaded = false;
    schedulePreload(0);
    emit lifecycleChanged();
}
