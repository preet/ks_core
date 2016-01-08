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


#include <catch/catch.hpp>

#include <ks/KsGlobal.hpp>
#include <ks/KsObject.hpp>
#include <ks/KsTimer.hpp>
#include <ks/KsTask.hpp>

using namespace ks;

// ============================================================= //
// ============================================================= //

void CountThenReturn(uint * count)
{
    (*count) += 1;
}

// ============================================================= //

TEST_CASE("EventLoop","[evloop]")
{
    uint count = 0;
    Milliseconds sleep_ms(10);
    auto count_then_ret = std::bind(CountThenReturn,&count);

    shared_ptr<EventLoop> event_loop = make_shared<EventLoop>();

    SECTION("PostEvents")
    {
        event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
        event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
        event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));

        SECTION(" ~ ")
        {
            event_loop.reset(); // Stop
            REQUIRE(count==0);
        }

        SECTION("Stop, Wait")
        {
            event_loop->Stop();
            event_loop->Wait(); // Should do nothing as the
            REQUIRE(count==0);  // event loop was never started
        }

        SECTION("PostStopEvent, Wait")
        {
            event_loop->PostStopEvent();
            event_loop->Wait(); // Should do nothing as the
            REQUIRE(count==0);  // event loop was never started
        }

        SECTION("ProcessEvents, Stop, Wait")
        {
            LOG.Info() << "KsTest: Expect ProcessEvent/Run "
                          "with inactive EventLoop error";

            REQUIRE_THROWS_AS(event_loop->ProcessEvents(),
                              EventLoopInactive);

            event_loop->Stop();
            event_loop->Wait();
            REQUIRE(count==0);
        }

        SECTION("Start, ProcessEvents")
        {
            event_loop->Start();
            event_loop->ProcessEvents();

            SECTION(" ~ ")
            {
                event_loop.reset();
                REQUIRE(count==3);
            }

            SECTION("Start")
            {
                // Expect multiple start calls to
                // do nothing to queued events
                event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
                event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
                event_loop->Start();
                REQUIRE(count==3);
            }

            SECTION("Stop","Wait")
            {
                event_loop->Stop();
                event_loop->Wait();
                REQUIRE(count==3);

                SECTION("Start, PostEvents, ProcessEvents, Stop, Wait")
                {
                    // restart
                    event_loop->Start();
                    event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
                    event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
                    event_loop->ProcessEvents();
                    event_loop->Stop();
                    event_loop->Wait();

                    REQUIRE(count==5);
                }
            }
        }

        SECTION("Start, ProcessEvents (thread)")
        {
            LOG.Info() << "KsTest: Expect ProcessEvents from "
                          "wrong thread error";

            event_loop->Start();
            event_loop->ProcessEvents();

            // Ensure that calling ProcessEvents from a second
            // thread without stopping the event loop is an error
            std::thread thread(
                        [event_loop]
                        () {
                            REQUIRE_THROWS_AS(event_loop->ProcessEvents(),
                                              EventLoopCalledFromWrongThread);
                        });

            thread.join();
        }

        SECTION("Run")
        {
            LOG.Info() << "KsTest: Expect ProcessEvent/Run "
                          "with inactive EventLoop error";

            REQUIRE_THROWS_AS(event_loop->Run(),EventLoopInactive);
            REQUIRE(count==0); // didn't call Start yet
        }

        SECTION("Start (thread), Run (thread)")
        {
            bool ok = false;
            std::thread thread = EventLoop::LaunchInThread(event_loop);

            SECTION("Stop")
            {
                event_loop->Stop();
                event_loop->Wait();
                thread.join();

                // count could be anything as any number of events
                // may have been processed before we call Stop

                // 'ok' could be anything as Stop in this thread might have
                // been called before Run is called in the spawned thread
            }

            SECTION("PostStopEvent,PostEvents")
            {
                event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
                event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
                event_loop->PostStopEvent();
                event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
                event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
                thread.join();
                REQUIRE(count==5);
            }

            SECTION("PostStopEvent, Wait")
            {
                event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
                event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
                event_loop->PostStopEvent();
                event_loop->Wait();
                REQUIRE(count==5);
                thread.join();
            }

            SECTION("PostStopEvent, Wait, Start, PostEvents, Stop")
            {
                event_loop->PostStopEvent();
                event_loop->Wait(); // should be stopped
                REQUIRE(count==3);

                event_loop->Start();
                event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
                event_loop->PostEvent(make_unique<SlotEvent>(count_then_ret));
                event_loop->Stop();
                event_loop->Wait();

                // The event loop is stopped with the initial PostStopEvent
                // and can't be restarted without calling Run() again (since
                // we're not using ProcessEvents here, its a blocking loop
                // in another thread); thus no more events should be invoked
                REQUIRE(count==3);
                thread.join();
            }
        }
    }
}

// ============================================================= //
// ============================================================= //


TEST_CASE("Tasks","[tasks]")
{
    bool ok = false;
    shared_ptr<EventLoop> evl = make_shared<EventLoop>();

    uint some_work=0;
    auto some_task =
            make_shared<Task>(
                [&some_work](){
                    for(uint i=0; i < 1000; i++) {
                        some_work++;
                    }
                });

    SECTION("Same thread")
    {
        evl->Start();
        evl->PostTask(some_task);
        REQUIRE(some_work==1000);

        // We didn't call evl->ProcessEvents so the
        // Task must have been Invoked during the
        // PostEvent call
    }

    SECTION("Different thread")
    {
        std::thread thread = EventLoop::LaunchInThread(evl);
        evl->PostTask(some_task);

        some_task->Wait();
        REQUIRE(some_work==1000);

        EventLoop::RemoveFromThread(evl,thread,true);
    }
}


// ============================================================= //
// ============================================================= //

class Derived0 : public Object
{
public:
    using base_type = ks::Object;

    Derived0(Object::Key const &key) :
        Object(key,nullptr)
    {
        m_create += "Construct0";
    }

    void Init(Object::Key const &,shared_ptr<Derived0> const &)
    {
        m_create += "Init0";
    }

    ~Derived0()
    {

    }

    std::string m_create;
};

class Derived1 : public Derived0
{
public:
    using base_type = Derived0;

    Derived1(Object::Key const &key) :
        Derived0(key)
    {
        m_create += "Construct1";
    }

    void Init(Object::Key const &,shared_ptr<Derived1> const &)
    {
        m_create += "Init1";
    }

    ~Derived1()
    {

    }
};

// ============================================================= //

TEST_CASE("Objects","[objects]")
{
    // Check construction and init order for make_object
    // with inheritance chains

    std::string const expect_create =
            "Construct0Construct1Init0Init1";

    shared_ptr<Derived1> d1 = make_object<Derived1>();
    bool ok = (expect_create == d1->m_create);

    REQUIRE(ok);
}

// ============================================================= //
// ============================================================= //

class TrivialReceiver : public Object
{
public:
    using base_type = ks::Object;

    TrivialReceiver(Object::Key const &key,
                    shared_ptr<EventLoop> event_loop) :
        Object(key,event_loop),
        invoke_count(0)
    {
        // empty
    }

    void Init(Object::Key const &,
              shared_ptr<TrivialReceiver> const &)
    {
        // empty
    }

    ~TrivialReceiver()
    {

    }

    void SlotCheck(bool * ok)
    {
        (*ok) = true;
    }

    void SlotCount()
    {
        invoke_count++;
    }

    void SlotSignalSelf(uint x, EventLoop * event_loop)
    {
        shared_ptr<TrivialReceiver> this_receiver =
                std::static_pointer_cast<TrivialReceiver>(
                    shared_from_this());

        if(x > 4) {
            event_loop->Stop();
            return;
        }

        Signal<uint,EventLoop*> signal_self;
        signal_self.Connect(
                    this_receiver,
                    &TrivialReceiver::SlotSignalSelf);

        // If the slot associated with signal_self is invoked
        // after the string append (ie. its queued), the string
        // would be 01234
        signal_self.Emit(x+1,event_loop);
        misc_string.append(ks::ToString(x));
    }

    void SlotSignalSelfBlocking(uint x, EventLoop * event_loop)
    {
        shared_ptr<TrivialReceiver> this_receiver =
                std::static_pointer_cast<TrivialReceiver>(
                    shared_from_this());

        if(x > 4) {
            event_loop->Stop();
            return;
        }

        Signal<uint,EventLoop*> signal_self;
        signal_self.Connect(
                    this_receiver,
                    &TrivialReceiver::SlotSignalSelfBlocking,
                    ConnectionType::Blocking);

        // If the slot associated with signal_self is invoked
        // after the string append (ie. its queued), the string
        // would be 01234
        signal_self.Emit(x+1,event_loop);
        misc_string.append(ks::ToString(x));
    }

    void SlotPrintAndCheckThreadId(std::string str,
                                   std::thread::id thread_id)
    {
        if(thread_id == std::this_thread::get_id()) {
            misc_string.append(str);
        }
    }

    void SlotThreadId()
    {
        thread_id = std::this_thread::get_id();
    }

    void SlotStopEventLoop(EventLoop * event_loop)
    {
        event_loop->Stop();
    }

    uint invoke_count;
    std::thread::id thread_id;

    std::string misc_string;
};



namespace test_signals
{
    uint g_counter{0};

    void Increment()
    {
        g_counter++;
    }

    class IncrementObject
    {
    public:
        void Increment()
        {
            g_counter++;
        }
    };
}

// ============================================================= //

TEST_CASE("Signals","[signals]")
{
    shared_ptr<EventLoop> event_loop = make_shared<EventLoop>();

    SECTION("Connect/Disconnect")
    {
        std::thread thread0 = EventLoop::LaunchInThread(event_loop);

        SECTION("Unmanaged Connections")
        {
            Signal<> signal_count;
            uint& counter = test_signals::g_counter;

            // lambda
            signal_count.Connect(
                        [&counter](){
                            counter++;
                        });

            // free function
            signal_count.Connect(
                        &test_signals::Increment);

            // member function
            test_signals::IncrementObject object;
            signal_count.Connect(
                        &object,
                        &test_signals::IncrementObject::Increment);

            signal_count.Emit();
            REQUIRE(counter == 3);
        }

        shared_ptr<TrivialReceiver> receiver =
                make_object<TrivialReceiver>(event_loop);

        Signal<bool*> signal_check;
        Signal<EventLoop*> signal_stop;

        auto cid0 = signal_check.Connect(
                    receiver,
                    &TrivialReceiver::SlotCheck);

        auto cid1 = signal_stop.Connect(
                    receiver,
                    &TrivialReceiver::SlotStopEventLoop);

        // ensure the connection succeeded by
        // testing the slots
        bool ok = false;
        signal_check.Emit(&ok);
        signal_stop.Emit(event_loop.get());
        event_loop->Wait();
        REQUIRE(ok);

        thread0.join();


        // try disconnecting
        REQUIRE(signal_check.Disconnect(cid0));

        // verify disconnection by testing the slots

        // restart event loop
        std::thread thread1 = EventLoop::LaunchInThread(event_loop);

        ok = false;
        signal_check.Emit(&ok);
        signal_stop.Emit(event_loop.get());
        event_loop->Wait();
        REQUIRE_FALSE(ok);

        // require repeat disconnects to fail
        REQUIRE_FALSE(signal_check.Disconnect(cid0));

        // require Disconnect on a non existant
        // connection id to fail
        REQUIRE_FALSE(signal_check.Disconnect(1234));

        thread1.join();
    }

    SECTION("Expired Connections")
    {
        std::thread thread = EventLoop::LaunchInThread(event_loop);

        Signal<bool*> signal_check;
        Id cid0;

        {
            shared_ptr<TrivialReceiver> temp_receiver =
                    make_object<TrivialReceiver>(event_loop);

            cid0 = signal_check.Connect(
                        temp_receiver,
                        &TrivialReceiver::SlotCheck);
        }

        // Check that the connections were
        // made successfully
        REQUIRE(signal_check.ConnectionValid(cid0));

        // Emit should remove expired connections
        bool ok;
        signal_check.Emit(&ok);
        REQUIRE_FALSE(signal_check.ConnectionValid(cid0));

        event_loop->Stop();
        thread.join();
    }

    SECTION("One-to-one and one-to-many connections")
    {
        std::thread thread0 = EventLoop::LaunchInThread(event_loop);

        // signals
        Signal<> signal_count;
        Signal<EventLoop*> signal_stop;

        // r0
        shared_ptr<TrivialReceiver> r0 =
                make_object<TrivialReceiver>(event_loop);

        size_t const one_one_count = 100;

        // test 1 signal -> 1 slot
        r0->invoke_count = 0;

        signal_count.Connect(r0,&TrivialReceiver::SlotCount);
        signal_stop.Connect(r0,&TrivialReceiver::SlotStopEventLoop);

        for(uint i=0; i < one_one_count; i++) {
            signal_count.Emit();
        }
        signal_stop.Emit(event_loop.get()); // stop event loop
        event_loop->Wait();
        REQUIRE(r0->invoke_count == one_one_count);

        thread0.join();


        // test 1 signal -> 4 slots

        // add receivers
        shared_ptr<TrivialReceiver> r1 = make_object<TrivialReceiver>(event_loop);
        shared_ptr<TrivialReceiver> r2 = make_object<TrivialReceiver>(event_loop);
        shared_ptr<TrivialReceiver> r3 = make_object<TrivialReceiver>(event_loop);

        // zero count
        r0->invoke_count = 0;
        r1->invoke_count = 0;
        r2->invoke_count = 0;
        r3->invoke_count = 0;

        size_t const one_many_count=100;

        // restart event loop
        std::thread thread1 = EventLoop::LaunchInThread(event_loop);

        signal_count.Connect(r1,&TrivialReceiver::SlotCount);
        signal_count.Connect(r2,&TrivialReceiver::SlotCount);
        signal_count.Connect(r3,&TrivialReceiver::SlotCount);

        for(uint i=0; i < one_many_count; i++) {
            signal_count.Emit();
        }
        signal_stop.Emit(event_loop.get()); // stop event loop
        event_loop->Wait();

        REQUIRE((r0->invoke_count+
                 r1->invoke_count+
                 r2->invoke_count+
                 r3->invoke_count) == one_many_count*4);

        thread1.join();
    }

    SECTION("Connection types")
    {
        std::thread thread = EventLoop::LaunchInThread(event_loop);

        shared_ptr<TrivialReceiver> receiver =
                make_object<TrivialReceiver>(event_loop);

        SECTION("Direct connection")
        {
            // Ensure that the slot is invoked directly
            // by this thread by comparing thread ids
            Signal<> signal_set_thread_id;
            signal_set_thread_id.Connect(
                        receiver,
                        &TrivialReceiver::SlotThreadId,
                        ConnectionType::Direct);

            signal_set_thread_id.Emit();
            EventLoop::RemoveFromThread(event_loop,thread,true);

            bool const check_ok =
                    (std::this_thread::get_id() == receiver->thread_id);

            REQUIRE(check_ok);
        }

        SECTION("Queued connection / Same thread")
        {
            // Same thread
            Signal<uint,EventLoop*> signal_self;
            signal_self.Connect(
                        receiver,
                        &TrivialReceiver::SlotSignalSelf);

            signal_self.Emit(0,event_loop.get());
            thread.join();

            bool const check_ok =
                    (receiver->misc_string == "01234");
            REQUIRE(check_ok);
        }

        SECTION("Queued connection / Different thread")
        {
            // Different thread

            // Test to see that
            // 1. The slots are invoked in the right order
            //    relative to how the signals were sent
            // 2. The slots were invoked by the thread that
            //    is running event_loop

            Signal<std::string,std::thread::id> signal_str_threadid;
            signal_str_threadid.Connect(
                        receiver,
                        &TrivialReceiver::SlotPrintAndCheckThreadId);

            std::thread::id const check_id = thread.get_id();
            signal_str_threadid.Emit("h",check_id);
            signal_str_threadid.Emit("e",check_id);
            signal_str_threadid.Emit("l",check_id);
            signal_str_threadid.Emit("l",check_id);
            signal_str_threadid.Emit("o",check_id);

            EventLoop::RemoveFromThread(event_loop,thread,true);

            bool const check_ok =
                    (receiver->misc_string == "hello");

            REQUIRE(check_ok);
        }

        SECTION("Blocking connection / Same thread")
        {
            // Same thread
            Signal<uint,EventLoop*> signal_self;
            signal_self.Connect(
                        receiver,
                        &TrivialReceiver::SlotSignalSelfBlocking,
                        ConnectionType::Blocking);

            signal_self.Emit(0,event_loop.get());
            thread.join();

            bool const check_ok =
                    (receiver->misc_string == "43210");
            REQUIRE(check_ok);
        }

        SECTION("Blocking connection / Different thread")
        {
            Signal<> signal_count;
            signal_count.Connect(
                        receiver,
                        &TrivialReceiver::SlotCount,
                        ConnectionType::Blocking);

            // Calling Emit() should block this thread until
            // receivers corresponding slot is invoked. To test
            // this, we manually increment count in lockstep
            signal_count.Emit();        // count = 1
            receiver->invoke_count++;   // count = 2
            signal_count.Emit();        // count = 3
            receiver->invoke_count++;   // count = 4
            signal_count.Emit();        // count = 5
            receiver->invoke_count++;   // count = 6

            REQUIRE(receiver->invoke_count == 6);
            EventLoop::RemoveFromThread(event_loop,thread,true);
        }
    }
}


// ============================================================= //
// ============================================================= //

class WakeupReceiver : public Object
{
public:

    using base_type = ks::Object;
    WakeupReceiver(Object::Key const &key,
                   shared_ptr<EventLoop> event_loop) :
        Object(key,event_loop),
        m_wakeup_count(0),
        m_wakeup_limit(0),
        m_waiting(false)
    {
       // empty
    }

    void Init(Object::Key const &,
              shared_ptr<WakeupReceiver> const &)
    {

    }

    ~WakeupReceiver()
    {

    }

    void Prepare(uint wakeup_limit)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_waiting = true;
        m_wakeup_count = 0;
        m_wakeup_limit = wakeup_limit;
    }

    void Block()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        while(m_waiting) {
            m_cv.wait(lock);
        }
    }

    void OnSleepFor(Milliseconds sleep_ms)
    {
        std::this_thread::sleep_for(sleep_ms);
    }

    void OnWakeup()
    {
        m_wakeup_count++;

        if(m_wakeup_count < m_wakeup_limit) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_waiting = false;
        m_cv.notify_all();
    }

private:
    uint m_wakeup_count;
    uint m_wakeup_limit;
    bool m_waiting;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

// ============================================================= //

TEST_CASE("ks::Timer","[timers]") {

    SECTION("inactive tests: ") {
        shared_ptr<EventLoop> event_loop =
                make_shared<EventLoop>();

        shared_ptr<Timer> timer =
                make_object<Timer>(event_loop);

        SECTION("destroy inactive") {
            shared_ptr<Timer> timer = nullptr;
        }

        SECTION("stop inactive") {
            timer->Stop();
            shared_ptr<Timer> timer = nullptr;
        }

        SECTION("start/stop fuzz inactive") {
            timer->Start(Milliseconds(10),false);
            timer->Start(Milliseconds(10),false);
            timer->Stop();
            timer->Stop();
            timer->Start(Milliseconds(10),false);
            timer->Stop();
            timer->Start(Milliseconds(10),false);
            timer->Stop();
            shared_ptr<Timer> timer = nullptr;
        }
    }

    SECTION("single timer: ") {
        shared_ptr<EventLoop> event_loop =
                make_shared<EventLoop>();

        std::thread thread = EventLoop::LaunchInThread(event_loop);

        shared_ptr<Timer> timer =
                make_object<Timer>(event_loop);

        shared_ptr<WakeupReceiver> receiver =
                make_object<WakeupReceiver>(event_loop);

        timer->signal_timeout.Connect(
                    receiver,
                    &WakeupReceiver::OnWakeup);

        SECTION("single shot / sequential: ") {
            // single shot
            std::chrono::time_point<std::chrono::steady_clock> start,end;
            start = std::chrono::steady_clock::now();

            timer->Start(Milliseconds(50),false);
            receiver->Prepare(1); // wait for 1 timeout signal
            receiver->Block();

            end = std::chrono::steady_clock::now();
            Milliseconds interval_ms =
                    std::chrono::duration_cast<
                        Milliseconds
                    >(end-start);

            // The single shot timer should have marked the
            // timer inactive after timing out
            REQUIRE_FALSE(timer->GetActive());

            // The interval must be greater than the requested amount
            REQUIRE(interval_ms.count() >= 50);


            // sequential

            // Sequential calls to Start cancel the previous
            // timer and should almost always prevent it from
            // emitting a timeout signal. We provide a reasonable
            // interval (50+ms) so that subsequent calls cancel
            // the timer and expect only one timeout (the last one)
            start = std::chrono::steady_clock::now();

            receiver->Prepare(1); // wait for 1 timeout signal
            timer->Start(Milliseconds(50),false);
            timer->Start(Milliseconds(60),false);
            timer->Start(Milliseconds(70),false);
            receiver->Block();

            end = std::chrono::steady_clock::now();
            interval_ms =
                    std::chrono::duration_cast<
                        Milliseconds
                    >(end-start);

            // The single shot timer should have marked the
            // timer inactive after timing out
            REQUIRE_FALSE(timer->GetActive());

            // The interval must be greater than the requested amount
            REQUIRE(interval_ms.count() >= 70);

            // Cleanup
            event_loop->Stop();
            event_loop->Wait();
            thread.join();
        }

        SECTION("repeating: ") {
            std::chrono::time_point<std::chrono::steady_clock> start,end;
            start = std::chrono::steady_clock::now();

            receiver->Prepare(3); // wait for 3 timeout signals
            timer->Start(Milliseconds(33),true);
            receiver->Block();

            end = std::chrono::steady_clock::now();
            Milliseconds interval_ms =
                    std::chrono::duration_cast<
                        Milliseconds
                    >(end-start);

            // repeat timer should stay active until
            // reset or destroyed
            REQUIRE(timer->GetActive());

            // The interval must be greater than the requested amount
            REQUIRE(interval_ms.count() >= 99);

            // Cleanup
            event_loop->Stop();
            event_loop->Wait();
            thread.join();
        }

        SECTION("delayed start: ") {
            // We want to ensure that timers are started
            // immediately by the calling thread and not
            // delayed by posting their start as an event
            // in the actual event loop.

            // For example if there are two events:
            // [busy event 25ms] [timer start]

            // The 'busy event' that takes 25ms to complete
            // should not delay the start of the timer

            // emit a signal to put the receiver's event loop's
            // thread to sleep
            Signal<Milliseconds> SignalSleepFor;
            SignalSleepFor.Connect(
                        receiver,
                        &WakeupReceiver::OnSleepFor);

            std::chrono::time_point<std::chrono::steady_clock> start,end;
            start = std::chrono::steady_clock::now();

            SignalSleepFor.Emit(Milliseconds(25));
            timer->Start(Milliseconds(25),false);
            receiver->Prepare(1);
            receiver->Block();

            end = std::chrono::steady_clock::now();
            Milliseconds interval_ms =
                    std::chrono::duration_cast<
                        Milliseconds
                    >(end-start);

            // Cleanup
            event_loop->Stop();
            event_loop->Wait();
            thread.join();

            // The single shot timer should have marked the
            // timer inactive after timing out
            REQUIRE_FALSE(timer->GetActive());

            // The interval shouldn't have been affected
            // by the delay so it should be about 25ms
            bool ok = (interval_ms.count() >= 25) && (interval_ms.count() <= 30);
            REQUIRE(ok);
        }
    }
}

// ============================================================= //
// ============================================================= //
