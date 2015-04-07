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

    class Timer : public ks::Object
    {
        friend class TimeoutHandler;
        friend class EventLoop;

    public:
        using base_type = ks::Object;

        Timer(ks::Object::Key const &key,
              shared_ptr<EventLoop> event_loop);

        static void Init(ks::Object::Key const &,
                         shared_ptr<Timer> const &);

        ~Timer();

        std::chrono::milliseconds GetInterval() const;

        bool GetRepeating() const;

        bool GetActive() const;

        void Start(std::chrono::milliseconds interval_ms,
                   bool repeating);

        void Stop();

        Signal<> SignalTimeout;

    private:
        std::chrono::milliseconds m_interval_ms;
        bool m_repeating;
        std::atomic<bool> m_active;
    };

} // ks

#endif // KS_TIMER_H
