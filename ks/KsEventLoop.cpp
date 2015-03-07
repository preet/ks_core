/*
   Copyright (C) 2015 Preet Desai (preet.desai@gmail.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

// stl
#include <map>

// asio
#include <ks/thirdparty/asio/asio.hpp>

// ks
#include <ks/KsLog.h>
#include <ks/KsEvent.h>
#include <ks/KsTimer.h>
#include <ks/KsEventLoop.h>

namespace ks
{
    std::atomic<Id> EventLoop::s_id_counter(0);

    Id EventLoop::genId()
    {
        Id id = s_id_counter;
        s_id_counter++;
        return id;
    }

    // ============================================================= //

    struct TimerInfo
    {
        TimerInfo(Id id,
                  weak_ptr<Timer> timer,
                  asio::io_service & service,
                  std::chrono::milliseconds interval_ms,
                  bool repeat) :
            id(id),
            timer(timer),
            interval_ms(interval_ms),
            asio_timer(service,interval_ms),
            repeat(repeat),
            canceled(false)
        {
            // empty
        }

        Id id;
        weak_ptr<Timer> timer;
        std::chrono::milliseconds interval_ms;
        asio::steady_timer asio_timer;
        bool repeat;
        bool canceled;
    };

    // ============================================================= //

    class TimeoutHandler
    {
    public:
        TimeoutHandler(shared_ptr<TimerInfo> &timerinfo, bool move)
        {
            if(move) {
                m_timerinfo = std::move(timerinfo);
            }
            else {
                m_timerinfo = timerinfo;
            }
        }

        TimeoutHandler(TimeoutHandler const &other)
        {
            m_timerinfo = other.m_timerinfo;
        }

        TimeoutHandler(TimeoutHandler && other)
        {
            m_timerinfo = std::move(other.m_timerinfo);
        }

        void operator()(asio::error_code const &ec)
        {
            if((ec == asio::error::operation_aborted) ||
               m_timerinfo->canceled) {
                // The timer was canceled
                return;
            }

            auto timer = m_timerinfo->timer.lock();
            if(!timer) {
                // The ks::Timer object has been destroyed
                return;
            }

            // Emit the timeout signal
            timer->SignalTimeout.Emit();

            // If this is a repeating timer, post another timeout
            if(m_timerinfo->repeat) {
                TimerInfo * timerinfo = m_timerinfo.get();

                // TODO this should be adjusted based on the actual
                // amount of time that has elapsed (or maybe add a
                // 'drift correction' option?)
                timerinfo->asio_timer.expires_from_now(
                            timerinfo->interval_ms);

                timerinfo->asio_timer.async_wait(
                            TimeoutHandler(m_timerinfo,true));

//                m_timerinfo->asio_timer.async_wait(
//                            TimeoutHandler(m_timerinfo,false));
            }
            else {
                // mark inactive
                timer->m_active = false;
            }
        }

    private:
        shared_ptr<TimerInfo> m_timerinfo;
    };

    // ============================================================= //

    class EventHandler
    {
    public:
        EventHandler(unique_ptr<Event> &event,
                     asio::io_service * service) :
            m_event(std::move(event)),
            m_service(service)
        {
            // empty
        }

        ~EventHandler()
        {
            // empty
        }

        // NOTE: asio requires handlers to be copy constructible,
        // but we want to force them to be move only (as @event
        // is not meant to be a shared resource). Apparently a
        // workaround is to declare but not define the copy
        // constructor:
        // http://stackoverflow.com/questions/17211263/...
        // how-to-trick-boostasio-to-allow-move-only-handlers
        EventHandler(EventHandler const &other);

        EventHandler(EventHandler && other)
        {
            m_event = std::move(other.m_event);
            m_service = other.m_service;
        }

        void operator()()
        {
            auto const ev_type = m_event->GetType();

            if(ev_type == Event::Type::Slot) {
                SlotEvent * ev =
                        static_cast<SlotEvent*>(
                            m_event.get());
                ev->Invoke();
            }
            else if(ev_type == Event::Type::BlockingSlot) {
                BlockingSlotEvent * ev =
                        static_cast<BlockingSlotEvent*>(
                            m_event.get());
                ev->Invoke();
            }
        }

    private:
        unique_ptr<Event> m_event;
        asio::io_service * m_service;
    };

    // ============================================================= //

    // EventLoop implementation
    struct EventLoop::Impl
    {
        Impl()
        {
            // empty
        }

        asio::io_service m_asio_service;
        unique_ptr<asio::io_service::work> m_asio_work;
    };

    // ============================================================= //
    // ============================================================= //

    EventLoop::EventLoop() :
        m_id(genId()),
        m_started(false),
        m_running(false),
        m_impl(new Impl())
    {
        // empty
    }

    EventLoop::~EventLoop()
    {
        this->Stop();
    }

    Id EventLoop::GetId() const
    {
        return m_id;
    }

    std::thread::id EventLoop::GetThreadId()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_thread_id;
    }

    bool EventLoop::GetStarted()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_started;
    }

    bool EventLoop::GetRunning()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_running;
    }

    void EventLoop::Start()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if(m_started || m_impl->m_asio_work) {
            return;
        }

        m_impl->m_asio_service.reset();

        m_impl->m_asio_work.reset(
                    new asio::io_service::work(
                        m_impl->m_asio_service));

        this->setActiveThread();
        m_started = true;

        m_cv_started.notify_all();
    }

    void EventLoop::Run(bool* ok)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if(!(m_started && m_impl->m_asio_work)) {
                if(ok) { *ok = false; }
                return;
            }

            if(!checkActiveThread()) {
                if(ok) { *ok = false; }
                return;
            }

            if(ok) { *ok = true; }
            m_running = true;
            m_cv_running.notify_all();
        }

        m_impl->m_asio_service.run(); // blocks!
        m_running = false;
    }

    void EventLoop::Stop()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_impl->m_asio_work.reset(nullptr);
        m_impl->m_asio_service.stop();
        unsetActiveThread();
        m_started = false;
        m_cv_stopped.notify_all();
    }

    void EventLoop::Wait()
    {
        this->waitUntilStopped();
    }


    bool EventLoop::ProcessEvents()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if(!(m_started && m_impl->m_asio_work)) {
                LOG.Warn() << "EventLoop: ProcessEvents called but "
                              "event loop has not been started";
                return false;
            }

            if(!checkActiveThread()) {
                return false;
            }
        }
        m_impl->m_asio_service.poll();
        return true;
    }

    void EventLoop::PostEvent(unique_ptr<Event> event)
    {
        // Timer events are handled immediately instead of
        // posting them to the event queue to avoid delaying
        // their start and end times
        if(event->GetType() == Event::Type::StartTimer) {
            this->startTimer(
                        std::unique_ptr<StartTimerEvent>(
                            static_cast<StartTimerEvent*>(
                                event.release())));
        }
        else if(event->GetType() == Event::Type::StopTimer) {
            this->stopTimer(std::unique_ptr<StopTimerEvent>(
                                static_cast<StopTimerEvent*>(
                                    event.release())));
        }
        else {
            m_impl->m_asio_service.post(
                        EventHandler(
                            event,
                            &(m_impl->m_asio_service)));
        }
    }

    void EventLoop::PostStopEvent()
    {
        m_impl->m_asio_service.post(std::bind(&EventLoop::Stop,this));
    }

    std::thread EventLoop::LaunchInThread(shared_ptr<EventLoop> event_loop, bool *ok)
    {
        std::thread thread(
                    [event_loop,ok]
                    () {
                        event_loop->Start();
                        event_loop->Run(ok);
                    });

        event_loop->waitUntilRunning();
        return thread;
    }

    void EventLoop::RemoveFromThread(shared_ptr<EventLoop> event_loop,
                                     std::thread &thread,
                                     bool post_stop)
    {
        if(post_stop) {
            event_loop->PostStopEvent();
        }
        else {
            event_loop->Stop();
        }

        thread.join();
    }

    void EventLoop::waitUntilStarted()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        while(!m_started) {
            m_cv_started.wait(lock);
        }
    }

    void EventLoop::waitUntilRunning()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        while(!m_running) {
            m_cv_running.wait(lock);
        }
    }

    void EventLoop::waitUntilStopped()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        while(m_started) {
            m_cv_stopped.wait(lock);
        }
    }

    void EventLoop::setActiveThread()
    {
        auto const calling_thread_id = std::this_thread::get_id();
        m_thread_id = calling_thread_id;
    }

    bool EventLoop::checkActiveThread()
    {
        auto const calling_thread_id = std::this_thread::get_id();

        // Ensure that the thread is this event loop's
        // active thread
        if(m_thread_id != calling_thread_id) {
            LOG.Error() << "EventLoop: ProcessEvents/Run should only "
                           "be called from the thread that called Start";
            return false;
        }
        return true;
    }

    void EventLoop::unsetActiveThread()
    {
        m_thread_id = m_thread_id_null;
    }

    void EventLoop::startTimer(unique_ptr<StartTimerEvent> ev)
    {
        // lock because we modify m_list_timers
        std::unique_lock<std::mutex> lock(m_mutex);

        auto timer = ev->GetTimer().lock();
        if(!timer) {
            // The timer object was destroyed
            return;
        }

        auto timerinfo_it = m_list_timers.find(ev->GetTimerId());
        if(timerinfo_it != m_list_timers.end()) {
            // If a timer for the given id already exists, erase it
            timerinfo_it->second->asio_timer.cancel();
            timerinfo_it->second->canceled = true;
            m_list_timers.erase(timerinfo_it);
        }

        // Insert a new timer and start it
        timerinfo_it = m_list_timers.emplace(
                    ev->GetTimerId(),
                    make_shared<TimerInfo>(
                        ev->GetTimerId(),
                        ev->GetTimer(),
                        m_impl->m_asio_service,
                        ev->GetInterval(),
                        ev->GetRepeating())).first;

        timer->m_active = true;
        timerinfo_it->second->asio_timer.async_wait(
                    TimeoutHandler(timerinfo_it->second,false));
    }

    void EventLoop::stopTimer(unique_ptr<StopTimerEvent> ev)
    {
        // lock because we modify m_list_timers
        std::unique_lock<std::mutex> lock(m_mutex);

        // Cancel and remove the timer for the given id
        auto timerinfo_it = m_list_timers.find(ev->GetTimerId());
        if(timerinfo_it == m_list_timers.end()) {
            return;
        }

        auto timer = timerinfo_it->second->timer.lock();
        if(timer) {
            timer->m_active = false;
        }

        timerinfo_it->second->asio_timer.cancel();
        timerinfo_it->second->canceled = true;
        m_list_timers.erase(timerinfo_it);
    }

} // ks
