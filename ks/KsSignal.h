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

#ifndef KS_SIGNAL_H
#define KS_SIGNAL_H

#include <functional>
#include <vector>
#include <mutex>
#include <utility>
#include <unordered_map>
#include <set>
#include <atomic>
#include <type_traits>
#include <algorithm>

#include <ks/KsEvent.h>
#include <ks/KsObject.h>

namespace ks
{
    enum class ConnectionType
    {
        Direct,
        Queued,
        BlockingQueued
    };

    template<typename... Args>
    class Signal final
    {
    private:
        struct Connection
        {
            Id id;
            ConnectionType type;
            weak_ptr<Object> receiver;
            std::function<void(Args&...)> slot;
        };

    public:
        Signal()
        {
            // empty
        }

        ~Signal()
        {

        }

        // NOTE: SlotArgs is a separate template parameter
        // here because it might not be equal to Signal<Args...>;
        // consider: Signal<Thing> and Slot(Thing const &)

        // Connect to a an event receiver's member function
        // TODO: Connect for free functions
        // TODO: should we be passing a weak_ptr instead
        //       of a shared_ptr? it should be clear that
        //       shared_ptr isn't saved; doing so would
        //       probably cause a bunch of circular dependency
        //       chains which would be very very bad. At the
        //       same time there's no point in passing a receiver
        //       thats already null to Connect which would be
        //       possible with a weak_ptr
        template<typename T, typename... SlotArgs>
        Id Connect(shared_ptr<T> const &receiver,
                   void (T::*slot)(SlotArgs...),
                   ConnectionType type=ConnectionType::Queued)
        {
            static_assert(std::is_base_of<Object,T>::value,
                          "KS: Signal::Connect(): "
                          "Type must be derived from ks::Object");

            // Wrap the function in a lambda and save it along
            // with the receiver in the list of connections
            std::lock_guard<std::mutex> lock(m_connection_mutex);
            weak_ptr<T> rcvr_weak_ptr(receiver);

            auto id = genId();
            m_list_connections.emplace_back(
                        Connection{
                            id,
                            type,
                            receiver,               // receiver
                            [rcvr_weak_ptr,slot]    // lambda to call slot
                            (Args&... args) {
                                auto rcvr = rcvr_weak_ptr.lock();
                                if(rcvr) {
                                    ((rcvr.get())->*slot)(args...);
                                }
                        }});

            return id;
        }

        bool Disconnect(Id connection_id)
        {
            std::lock_guard<std::mutex> lock(m_connection_mutex);

            auto connection_it = findConnection(connection_id);

            if(connection_it != m_list_connections.end()) {
                m_list_connections.erase(connection_it);
                return true;
            }
            return false;
        }

        void Emit(Args const &... args)
        {
            // TODO: Maybe provide a Non-threadsafe signal class
            // (maybe use a dummy mutex) so we can avoid mutex
            // overhead on direct connections

            // Go through each connection and post an event
            // to invoke the slot with @args

            std::lock_guard<std::mutex> lock(m_connection_mutex);

            uint expired_count=0;

            for(auto &connection : m_list_connections) {
                auto receiver = connection.receiver.lock();
                if(receiver) {
                    if(connection.type == ConnectionType::Direct) {
                        // invoke the slot directly
                        directInvoke(args...,connection);
                    }
                    else if(connection.type == ConnectionType::Queued) {
                        // post the slot to the receivers thread

                        // NOTE: passing std::bind to make_unique
                        // here causes an extra move
                        unique_ptr<Event> event(new SlotEvent(
                            std::bind(connection.slot,args...)));

                        receiver->GetEventLoop()->PostEvent(std::move(event));
                    }
                    else {
                        // post the slot to the receivers thread
                        // and block until its invoked
                        bool invoked = false;
                        std::mutex invoked_mutex;
                        std::condition_variable invoked_cv;

                        unique_ptr<Event> event(new BlockingSlotEvent(
                            std::bind(connection.slot,args...),
                            &invoked,
                            &invoked_mutex,
                            &invoked_cv));

                        std::unique_lock<std::mutex> invoked_lock(invoked_mutex);
                        receiver->GetEventLoop()->PostEvent(std::move(event));

                        while(!invoked) {
                            invoked_cv.wait(invoked_lock);
                        }
                    }
                }
                else {
                    // If the receiver for this connection has been
                    // destroyed, mark the connection as expired
                    expired_count++;
                }
            }

            // Remove any expired connections
            if(expired_count > 0) {
                auto remove_begin = std::remove_if(
                            m_list_connections.begin(),
                            m_list_connections.end(),
                            [](Connection const &connection) {
                                return (connection.receiver.expired());
                            });

                m_list_connections.erase(
                            remove_begin,
                            m_list_connections.end());
            }
        }

        bool ConnectionValid(Id cid)
        {
            std::lock_guard<std::mutex> lock(m_connection_mutex);

            if(findConnection(cid) != m_list_connections.end()) {
                return true;
            }
            return false;
        }

    private:
        void directInvoke(Args... args, Connection &connection)
        {
            connection.slot(args...);
        }

        // * not thread safe, lock @m_connection_mutex
        //   before calling this method
        typename std::vector<Connection>::iterator
        findConnection(Id connection_id)
        {
            auto connection_it = std::find_if(
                        m_list_connections.begin(),
                        m_list_connections.end(),
                        [connection_id]
                        (Connection const &connection) {
                            return (connection.id == connection_id);
                        });

            return connection_it;
        }

        // Connections
        std::mutex m_connection_mutex;
        std::vector<Connection> m_list_connections;

        static std::atomic<Id> s_connection_id_counter;
        static Id genId() {
            Id id = s_connection_id_counter;
            s_connection_id_counter++;
            return id;
        }
    };

    // TODO do we need this stuff anymore?
    template<typename... Args>
    std::atomic<Id> Signal<Args...>::s_connection_id_counter(0);

    // NOTE: --- start bug workaround ---
    // For Signal::Emit(...)
    // gcc (at least until 4.8.2) has a bug that
    // prevents it from capturing parameter packs
    // with lambdas, so we use std::bind instead

    // TODO: bind might be slower so maybe prefer
    // that only if parameter pack lambda capture
    // isn't supported? (I think bind requires an
    // extra move to convert -> function<void()>)

    // auto s = make_unique<SlotEvent>(
    //             [connection, args...]() {
    //     connection.slot(args...);
    // });
    // -- end bug work around ---

} // ks

#endif // KS_SIGNAL_H
