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

#ifndef KS_SIGNAL_HPP
#define KS_SIGNAL_HPP

#include <functional>
#include <vector>
#include <mutex>
#include <utility>
#include <type_traits>
#include <algorithm>

#include <ks/KsEvent.hpp>
#include <ks/KsObject.hpp>

namespace ks
{
    // ============================================================= //

    enum class ConnectionType : u8
    {
        Direct,
        Queued,
        Blocking
    };

    namespace signal_detail
    {
        // connection id
        extern Id g_cid_counter;
        extern std::mutex g_cid_mutex;

        Id genId();

    } // signal_detail

    // ============================================================= //

    class SignalMutex // implements BasicLockable requirements
    {
    public:
        SignalMutex() { }
        virtual ~SignalMutex() { }

        virtual void lock()=0;
        virtual void unlock()=0;
    };

    class DefaultSignalMutex final : public SignalMutex
    {
    public:
        ~DefaultSignalMutex() { }
        void lock() { m_mutex.lock(); }
        void unlock() { m_mutex.unlock(); }

    private:
        std::mutex m_mutex;
    };

    class DummySignalMutex final : public SignalMutex
    {
    public:
        ~DummySignalMutex() { }
        void lock() { }
        void unlock() { }
    };

    // ============================================================= //

    // * A simple empty ks::Object that can be used for a
    //   signal's connection context.
    // * Managed connections use the context object's EventLoop
    //   for invoking Queued and Blocking methods
    // * Managed connections expire (are automatically removed)
    //   when the context object is deleted.
    class ConnectionContext : public ks::Object
    {
    public:
        using base_type = ks::Object;

        ConnectionContext(ks::Object::Key const &key,
                         shared_ptr<EventLoop> evl) :
            ks::Object(key,std::move(evl))
        {}

        void Init(ks::Object::Key const &,
                  shared_ptr<ConnectionContext> const &)
        {}

        ~ConnectionContext() = default;
    };

    // ============================================================= //

    template<typename... Args>
    class Signal final
    {
        struct ManagedConnection
        {
            Id id;
            ConnectionType type;
            weak_ptr<Object> context;
            std::function<void(Args&...)> fn;
        };

        struct UnmanagedConnection
        {
            Id id;
            std::function<void(Args&...)> fn;
        };

    public:       
        Signal(unique_ptr<SignalMutex> connection_mutex=
               make_unique<DefaultSignalMutex>()) :
            m_connection_mutex(std::move(connection_mutex))
        {}

        ~Signal()
        {}

        template<typename FunctionType>
        Id Connect(FunctionType fn,
                   shared_ptr<Object> const &context=nullptr,
                   ConnectionType type=ConnectionType::Queued)
        {
            std::lock_guard<SignalMutex> lock(*m_connection_mutex);
            auto id = signal_detail::genId();

            if(context) {
                weak_ptr<Object> ctx(context);
                m_list_managed_connections.emplace_back(
                            ManagedConnection{
                                id,
                                type,
                                ctx,
                                [fn,ctx](Args&... args) {
                                    auto is_alive = ctx.lock();
                                    if(is_alive) {
                                        fn(args...);
                                    }
                                }
                            });
            }
            else {
                m_list_unmanaged_connections.emplace_back(
                            UnmanagedConnection{
                                id,fn
                            });
            }

            return id;
        }

        // NOTE: Fn/SlotArgs is a separate template parameter
        // here because it might not be equal to Signal<Args...>;
        // consider: Signal<Thing> and Slot(Thing const &)
        template<typename T, typename... FnArgs>
        Id Connect(T* object,
                   void(T::*memfn)(FnArgs...),
                   shared_ptr<Object> const &context=nullptr,
                   ConnectionType type=ConnectionType::Queued)
        {
            std::lock_guard<SignalMutex> lock(*m_connection_mutex);
            auto id = signal_detail::genId();

            if(context) {
                weak_ptr<Object> ctx(context);
                m_list_managed_connections.emplace_back(
                            ManagedConnection{
                                id,
                                type,
                                ctx,
                                [object,memfn,ctx](Args&... args) {
                                    auto is_alive = ctx.lock();
                                    if(is_alive) {
                                        (object->*memfn)(args...);
                                    }
                                }
                            });
            }
            else {
                m_list_unmanaged_connections.emplace_back(
                            UnmanagedConnection{
                                id,
                                [object,memfn](Args&... args) {
                                    (object->*memfn)(args...);
                                }
                            });
            }

            return id;
        }

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
            std::lock_guard<SignalMutex> lock(*m_connection_mutex);
            weak_ptr<T> rcvr_weak_ptr(receiver);

            auto id = signal_detail::genId();
            m_list_managed_connections.emplace_back(
                        ManagedConnection{
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
            std::lock_guard<SignalMutex> lock(*m_connection_mutex);

            auto managed_cnxn_it = findManagedConnection(connection_id);
            if(managed_cnxn_it != m_list_managed_connections.end())
            {
                m_list_managed_connections.erase(managed_cnxn_it);
                return true;
            }

            auto unmanaged_cnxn_it = findUnmanagedConnection(connection_id);
            if(unmanaged_cnxn_it != m_list_unmanaged_connections.end())
            {
                m_list_unmanaged_connections.erase(unmanaged_cnxn_it);
                return true;
            }

            return false;
        }

        void Emit(Args const &... args)
        {
            // Go through each connection and post an event
            // to invoke the slot with @args

            std::lock_guard<SignalMutex> lock(*m_connection_mutex);

            // Invoke unmananged connections
            for(auto& connection : m_list_unmanaged_connections)
            {
                directInvoke(args...,connection.fn);
            }

            // Invoke/Schedule managed connections
            uint expired_count=0;
            for(auto& connection : m_list_managed_connections)
            {
                auto context = connection.context.lock();

                if(context==nullptr)
                {
                    // If the receiver for this connection has been
                    // destroyed, mark the connection as expired
                    expired_count++;
                    continue;
                }

                if(connection.type == ConnectionType::Direct)
                {
                    directInvoke(args...,connection.fn);
                }
                else if(connection.type == ConnectionType::Queued)
                {
                    // Post the slot to the receivers thread
                    unique_ptr<Event> event(new SlotEvent(
                        std::bind(connection.fn,args...)));

                    context->GetEventLoop()->PostEvent(std::move(event));
                }
                else // ConnectionType::Blocking
                {
                    // Check if the receiver event loop is active

                    // NOTE: Foregoing this check will result in deadlock if
                    // the event loop is inactive. The check is not mandatory
                    // if it can be guaranteed no blocking signals will be
                    // emitted before the required event loops have started.

                    std::thread::id evl_thread_id;
                    bool evl_started;
                    bool evl_running;
                    context->GetEventLoop()->GetState(evl_thread_id,
                                                      evl_started,
                                                      evl_running);

                    if(!evl_started) {
                        // TODO: Add a test for this case
                        throw EventLoopInactive(
                                    "Signal: Attempted to emit a Blocking "
                                    "signal connected to a receiver with "
                                    "an inactive event loop");
                    }

                    if(evl_thread_id == std::this_thread::get_id()) {
                        // TODO:
                        // We could potentially process any queued events
                        // here first before invoking the slot:
                        //
                        // receiver->GetEventLoop()->ProcessEvents();
                        //
                        // However:
                        // * Processing events might cause this signal's
                        //   Emit() to be called recursively which could
                        //   cause a deadlock (we could work around this
                        //   by using recursive mutexes though)
                        // * Chains of blocking queued signals result in
                        //   multiple levels of recursion

                        // invoke this slot directly
                        directInvoke(args...,connection.fn);
                    }
                    else {
                        // post the slot to the receivers thread
                        // and block until its invoked
                        bool invoked = false;
                        std::mutex invoked_mutex;
                        std::condition_variable invoked_cv;

                        unique_ptr<Event> event(new BlockingSlotEvent(
                            std::bind(connection.fn,args...),
                            &invoked,
                            &invoked_mutex,
                            &invoked_cv));

                        std::unique_lock<std::mutex> invoked_lock(invoked_mutex);
                        context->GetEventLoop()->PostEvent(std::move(event));

                        while(!invoked) {
                            invoked_cv.wait(invoked_lock);
                        }
                    }
                }
            }

            // Remove any expired connections
            if(expired_count > 0) {
                auto remove_begin = std::remove_if(
                            m_list_managed_connections.begin(),
                            m_list_managed_connections.end(),
                            [](ManagedConnection const &connection) {
                                return (connection.context.expired());
                            });

                m_list_managed_connections.erase(
                            remove_begin,
                            m_list_managed_connections.end());
            }
        }

        bool ConnectionValid(Id connection_id)
        {
            std::lock_guard<SignalMutex> lock(*m_connection_mutex);

            auto managed_cnxn_it = findManagedConnection(connection_id);
            if(managed_cnxn_it == m_list_managed_connections.end())
            {
                auto unmanaged_cnxn_it = findUnmanagedConnection(connection_id);
                if(unmanaged_cnxn_it == m_list_unmanaged_connections.end())
                {
                    return false;
                }
            }

            return true;
        }

        uint GetConnectionCount()
        {
            std::lock_guard<SignalMutex> lock(*m_connection_mutex);
            return m_list_managed_connections.size()+
                   m_list_unmanaged_connections.size();
        }

    private:
        void directInvoke(Args... args,std::function<void(Args&...)> &fn)
        {
            fn(args...);
        }

        typename std::vector<ManagedConnection>::iterator
        findManagedConnection(Id connection_id)
        {
            auto connection_it = std::find_if(
                        m_list_managed_connections.begin(),
                        m_list_managed_connections.end(),
                        [connection_id]
                        (ManagedConnection const &connection) {
                            return (connection.id == connection_id);
                        });

            return connection_it;
        }

        typename std::vector<UnmanagedConnection>::iterator
        findUnmanagedConnection(Id connection_id)
        {
            auto connection_it = std::find_if(
                        m_list_unmanaged_connections.begin(),
                        m_list_unmanaged_connections.end(),
                        [connection_id]
                        (UnmanagedConnection const &connection) {
                            return (connection.id == connection_id);
                        });

            return connection_it;
        }

        // Connections
        unique_ptr<SignalMutex> m_connection_mutex;
        std::vector<ManagedConnection> m_list_managed_connections;
        std::vector<UnmanagedConnection> m_list_unmanaged_connections;
    };

    // ============================================================= //

} // ks

#endif // KS_SIGNAL_HPP
