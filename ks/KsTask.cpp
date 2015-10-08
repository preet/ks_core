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

#include <ks/KsTask.hpp>
#include <ks/KsLog.hpp>

namespace ks
{
    Task::Task(std::function<void()> task) :
        m_task(std::move(task)),
        m_future(m_promise.get_future()),
        m_complete(false)
    {

    }

    Task::~Task()
    {

    }

    void Task::Invoke()
    {
        // TODO throw if already complete?

        m_task();
        m_promise.set_value();
        m_complete = true;
    }

    Task::WaitStatus Task::Wait()
    {
        if(m_complete) {
            return WaitStatus::Finished;
        }

        // We assume that m_future is valid since we set
        // it during construction and never call get() on it
        m_future.wait();
        return WaitStatus::Ready;
    }

    Task::WaitStatus Task::WaitFor(std::chrono::milliseconds wait_ms)
    {      
        if(m_complete) {
            return WaitStatus::Finished;
        }

        // We assume that m_future is valid since we set
        // it during construction and never call get() on it
        auto fs = m_future.wait_for(wait_ms);

        if(fs == std::future_status::ready) {
            return WaitStatus::Ready;
        }

        // TODO: Do we have to check for future_status::deferred?

        // std::future_status::timeout
        return WaitStatus::Timeout;
    }
}
