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
        enum class WaitStatus
        {
            Finished,
            Ready,
            Timeout
        };

        Task(std::function<void()> task);

        ~Task();

        void Invoke();

        // Wait on a task indefinitely.
        // NOTE: Do NOT use the WaitFor function that takes wait_ms
        // as an argument to try and wait indefinitely (ie. by setting
        // wait_ms = milliseconds::max()) as this will cause an
        // overflow (wait_for(t) == wait_until(now + t))!
        WaitStatus Wait();

        // Wait on a task for wait_ms milliseconds
        WaitStatus WaitFor(std::chrono::milliseconds wait_ms);

    private:
        std::function<void()> m_task;
        std::promise<void> m_promise;
        std::future<void> m_future;
        std::atomic<bool> m_complete;
    };


} // ks

#endif // KS_TASK_H
