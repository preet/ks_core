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

#ifndef KS_TASK_H
#define KS_TASK_H

#include <functional>
#include <condition_variable>
#include <future>

#include <ks/KsGlobal.h>

namespace ks
{
    class Task final
    {
    public:
        Task(std::function<void()> task) :
            m_task(std::move(task)),
            m_future(m_promise.get_future()),
            m_complete(false),
            m_thread_id(std::this_thread::get_id())
        {

        }

        ~Task()
        {

        }

        void Invoke()
        {
            m_task();
            m_promise.set_value();
            m_complete = true;
        }

        uint Wait(std::chrono::milliseconds wait_ms=
                  std::chrono::milliseconds::max())
        {
            // m_future should always be valid as we
            // set it during construction and never
            // call get() on it (only wait())
            // TODO do we care about std::future_status::deferred?
            if((!m_complete) && m_future.valid()) {
                auto fs = m_future.wait_for(wait_ms);
                if(fs == std::future_status::ready) {
                    return 1;
                }
                if(fs == std::future_status::timeout) {
                    return 2;
                }
            }
            return 0;
        }

        std::thread::id GetThreadId() const
        {
            return m_thread_id;
        }

    private:
        std::function<void()> m_task;
        std::promise<void> m_promise;
        std::future<void> m_future;
        std::atomic<bool> m_complete;
        std::thread::id const m_thread_id;
    };


} // ks

#endif // KS_TASK_H
