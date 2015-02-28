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

#include <ks/KsApplication.h>
#include <ks/KsLog.h>

namespace ks
{
    Application::Application() :
        Object(make_shared<EventLoop>()),
        m_sys_thread_id(std::this_thread::get_id())
    {
        // empty
    }

    Application::~Application()
    {
        // empty
    }

    void Application::AddCleanupRequest(shared_ptr<Object> const &obj)
    {
        std::lock_guard<std::mutex> lock(m_mutex_list_cleanup_objs);
        m_list_cleanup_objs.push_back(obj);
    }

    void Application::OnFinishedCleanup(Id object_id)
    {
        auto obj_it = std::find_if(
                    m_list_cleanup_objs.begin(),
                    m_list_cleanup_objs.end(),
                    [object_id]
                    (shared_ptr<Object> const &obj) {
                        return (obj->GetId() == object_id);
                    });

        if(obj_it == m_list_cleanup_objs.end()) {
            LOG.Error() << "Application::OnFinishedCleanup(): "
                           "object_id " << object_id << "DNE!";
            return;
        }

        m_list_cleanup_objs.erase(obj_it);

        if(m_list_cleanup_objs.empty()) {
            // unlock m_mutex_list_cleanup_objs?
            this->quit(0);
        }
    }

    void Application::startCleanup()
    {
        // block any more cleanup requests from being added
        m_mutex_list_cleanup_objs.lock(); // TODO is it safe to leave this locked?

        if(m_list_cleanup_objs.empty()) {
            this->quit(0);
        }
        else {
            SignalStartCleanup.Emit();
        }
    }

    void Application::init()
    {

    }
} // ks
