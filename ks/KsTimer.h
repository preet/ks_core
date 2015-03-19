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

#ifndef KS_TIMER_H
#define KS_TIMER_H

#include <ks/KsObject.h>
#include <ks/KsSignal.h>

namespace ks
{
    static_assert(std::chrono::high_resolution_clock::duration::period::den >= 1000,
                  "ERROR: std::chrono::high_resolution_clock's duration period "
                  "resolution must match or be better than a millisecond");

    class Timer : public Object
    {
        friend class TimeoutHandler;
        friend class EventLoop;

    public:
        using base_type = Object;

        Timer(Object::Key const &key,
              shared_ptr<EventLoop> event_loop) :
            Object(key,event_loop),
            m_interval_ms(0),
            m_repeating(false),
            m_active(false)
        {

        }

        static void Init(Object::Key const &,
                         shared_ptr<Timer>)
        {

        }

        ~Timer()
        {
            Stop();
        }

        std::chrono::milliseconds GetInterval() const
        {
            return m_interval_ms;
        }

        bool GetRepeating() const
        {
            return m_repeating;
        }

        bool GetActive() const
        {
            return m_active;
        }

        void Start(std::chrono::milliseconds interval_ms,
                   bool repeating)
        {
            m_interval_ms = interval_ms;
            m_repeating = repeating;

            shared_ptr<Timer> this_timer =
                    std::static_pointer_cast<Timer>(
                        shared_from_this());

            unique_ptr<Event> timer_event =
                    make_unique<StartTimerEvent>(
                        this->GetId(),
                        this_timer,
                        m_interval_ms,
                        m_repeating);

            this->GetEventLoop()->PostEvent(
                        std::move(timer_event));
        }

        void Stop()
        {
            unique_ptr<Event> timer_event =
                    make_unique<StopTimerEvent>(
                        this->GetId());

            this->GetEventLoop()->PostEvent(
                        std::move(timer_event));
        }

        Signal<> SignalTimeout;

    private:
        std::chrono::milliseconds m_interval_ms;
        bool m_repeating;
        std::atomic<bool> m_active;
    };

} // ks

#endif // KS_TIMER_H
