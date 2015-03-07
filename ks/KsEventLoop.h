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

#ifndef KS_EVENT_LOOP_H
#define KS_EVENT_LOOP_H

#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include <vector>
#include <condition_variable>

#include <ks/KsGlobal.h>

namespace ks
{
    class Event;
    class StartTimerEvent;
    class StopTimerEvent;
    class TimerInfo;

    class EventLoop final
    {
        class Impl; // hides the implementation

    public:
        EventLoop();
        EventLoop(EventLoop const &other) = delete;
        EventLoop(EventLoop &&other) = delete;
        virtual ~EventLoop();

        EventLoop & operator = (EventLoop const &) = delete;
        EventLoop & operator = (EventLoop &&) = delete;

        Id GetId() const;
        std::thread::id GetThreadId();
        bool GetStarted();
        bool GetRunning();

        void Start();
        void Run(bool *ok=nullptr);
        void Stop();
        void Wait();
        bool ProcessEvents();
        void PostEvent(unique_ptr<Event> event);
        void PostStopEvent();

        static std::thread LaunchInThread(shared_ptr<EventLoop> event_loop,
                                          bool * ok = nullptr);

        static void RemoveFromThread(shared_ptr<EventLoop> event_loop,
                                     std::thread & thread,
                                     bool post_stop=false);

        void waitUntilStarted();
        void waitUntilRunning();
        void waitUntilStopped();
    private:
        void startTimer(unique_ptr<StartTimerEvent> event);
        void stopTimer(unique_ptr<StopTimerEvent> event);
        void setActiveThread();
        void unsetActiveThread();
        bool checkActiveThread();

        Id const m_id;
        std::thread::id const m_thread_id_null; // default id for 'no thread'
        std::thread::id m_thread_id;

        bool m_started;
        bool m_running;
        std::mutex m_mutex;
        std::condition_variable m_cv_started;
        std::condition_variable m_cv_running;
        std::condition_variable m_cv_stopped;
        std::map<Id,shared_ptr<TimerInfo>> m_list_timers;

        shared_ptr<Impl> m_impl;

        static std::atomic<Id> s_id_counter;
        static Id genId();
    };
} // ks

#endif // KS_EVENT_LOOP_H
