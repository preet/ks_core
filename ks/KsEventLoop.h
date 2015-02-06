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
#include <condition_variable>

#include <ks/KsGlobal.h>

namespace ks
{
    class Event;
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
        bool GetStarted();

        void Start();
        void Run();
        void Stop();
        void Wait();
        void ProcessEvents();
        void PostEvent(unique_ptr<Event> event);
        void PostStopEvent();
        void WaitUntilStarted();
        void WaitUntilStopped();

    private:
        void waitUntilStarted();
        void waitUntilStopped();

        Id const m_id;

        bool m_started;
        std::mutex m_mutex;
        std::condition_variable m_cv_started;
        std::condition_variable m_cv_stopped;
        std::map<Id,shared_ptr<TimerInfo>> m_list_timers;

        shared_ptr<Impl> m_impl;

        static std::atomic<Id> s_id_counter;
        static Id genId();
    };
} // ks

#endif // KS_EVENT_LOOP_H
