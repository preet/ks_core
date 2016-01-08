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

#ifndef KS_EVENT_HPP
#define KS_EVENT_HPP

#include <functional>
#include <condition_variable>
#include <future>

#include <ks/KsGlobal.hpp>
#include <ks/KsLog.hpp>

namespace ks
{
    // Event
    class Event
    {
    public:
        enum class Type : u8
        {
            Null,
            Slot,
            BlockingSlot,
            StartTimer,
            StopTimer
        };

        virtual ~Event()
        {
            // empty
        }

        Type GetType() const {
            return m_type;
        }

    protected:

        Event(Type type) :
            m_type(type)
        {
            // empty
        }

        Event(Event const &) = delete;
        Event(Event &&) = delete;
        Event & operator = (Event const &) = delete;
        Event & operator = (Event &&) = delete;

    private:
        Type m_type;
    };

    // NullEvent
    class NullEvent : public Event
    {
    public:
        NullEvent() :
            Event(Event::Type::Null)
        {
            // empty
        }

        ~NullEvent()
        {
            // empty
        }
    };

    // TimerEvent
    class Timer;

    class StartTimerEvent : public Event
    {
    public:
        StartTimerEvent(Id timer_id,
                        weak_ptr<Timer> timer,
                        Milliseconds interval_ms,
                        bool repeating) :
            Event(Event::Type::StartTimer),
            m_timer_id(timer_id),
            m_timer(timer),
            m_interval_ms(interval_ms),
            m_repeating(repeating)
        {

        }

        ~StartTimerEvent()
        {

        }

        Id GetTimerId() const
        {
            return m_timer_id;
        }

        weak_ptr<Timer> GetTimer() const
        {
            return m_timer;
        }

        Milliseconds GetInterval() const
        {
            return m_interval_ms;
        }

        bool GetRepeating() const
        {
            return m_repeating;
        }

    private:
        Id const m_timer_id;
        weak_ptr<Timer> const m_timer;
        Milliseconds const m_interval_ms;
        bool const m_repeating;
    };

    class StopTimerEvent : public Event
    {
    public:
        StopTimerEvent(Id timer_id) :
            Event(Event::Type::StopTimer),
            m_timer_id(timer_id)
        {

        }

        ~StopTimerEvent()
        {

        }

        Id GetTimerId() const
        {
            return m_timer_id;
        }

    private:
        Id m_timer_id;
    };

    // SlotEvent
    class SlotEvent : public Event
    {
    public:
        SlotEvent(std::function<void()> &&slot) :
            Event(Event::Type::Slot),
            m_slot(std::move(slot))
        {
            // empty
        }

        ~SlotEvent()
        {
            // empty
        }

        void Invoke()
        {
            m_slot();
        }

    private:
        std::function<void()> m_slot;
    };

    class BlockingSlotEvent : public Event
    {
    public:
        BlockingSlotEvent(std::function<void()> &&slot,
                          bool * invoked,
                          std::mutex * invoked_mutex,
                          std::condition_variable * invoked_cv) :
            Event(Event::Type::BlockingSlot),
            m_slot(std::move(slot)),
            m_invoked(invoked),
            m_invoked_mutex(invoked_mutex),
            m_invoked_cv(invoked_cv)
        {
            // empty
        }

        ~BlockingSlotEvent()
        {
            // empty
        }

        void Invoke()
        {
            m_slot();

            // wake any waiting threads
            m_invoked_mutex->lock();
            (*m_invoked) = true;
            m_invoked_cv->notify_all();
            m_invoked_mutex->unlock();
        }

    private:
        std::function<void()> m_slot;

        bool * m_invoked;
        std::mutex * m_invoked_mutex;
        std::condition_variable * m_invoked_cv;
    };

} // ks

#endif // KS_EVENT_HPP
