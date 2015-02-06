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

#ifndef KS_OBJECT_H
#define KS_OBJECT_H

#include <vector>
#include <mutex>
#include <atomic>

#include <ks/KsGlobal.h>
#include <ks/KsEventLoop.h>

namespace ks
{
    class Event;

    class Object : public std::enable_shared_from_this<Object>
    {
    public:
        Object(shared_ptr<EventLoop> const &thread);
        Object(Object const &other) = delete;
        Object(Object &&other) = delete;
        virtual ~Object();

        Object & operator = (Object const &) = delete;
        Object & operator = (Object &&) = delete;

        Id GetId() const;
        shared_ptr<EventLoop> const & GetEventLoop() const;

    private:
        Id const m_id;

        shared_ptr<EventLoop> m_event_loop;
        static std::atomic<Id> s_id_counter;
        static Id genId();
    };

} // ks

#endif // KS_OBJECT_H
