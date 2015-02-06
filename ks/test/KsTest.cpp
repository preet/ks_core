#define CATCH_CONFIG_MAIN
#include <ks/thirdparty/catch/catch.hpp>

#include <ks/KsGlobal.h>
#include <ks/KsApplication.h>
#include <ks/KsTimer.h>

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
    std::chrono::milliseconds sleep_ms(10);
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
            event_loop->ProcessEvents();    // Should do nothing as the
            event_loop->Stop();             // event loop was never started
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

        SECTION("Run")
        {
            event_loop->Run();
            REQUIRE(count==0); // didn't call Start yet
        }

        SECTION("Start, Run (thread)")
        {
            event_loop->Start();

            std::thread thread(
                        [event_loop]
                        () {
                            event_loop->Run();
                        });

            SECTION("Stop")
            {
                event_loop->Stop();
                thread.join();
                // count could be anything as any
                // number of events may have been
                // processed before we call Stop
            }

            SECTION("PostStopEvent")
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

class TrivialReceiver : public Object
{
public:
    TrivialReceiver(shared_ptr<EventLoop> event_loop) :
        Object(event_loop),
        invoke_count(0)
    {
        // empty
    }

    void SlotCheck(bool * ok)
    {
        (*ok) = true;
    }

    void SlotCount()
    {
        invoke_count++;
    }

    void SlotStopEventLoop(EventLoop * event_loop)
    {
        event_loop->Stop();
    }

    uint invoke_count;
};

// ============================================================= //

TEST_CASE("Signals","[signals]")
{
    // Create and start event loop
    shared_ptr<EventLoop> event_loop = make_shared<EventLoop>();

    shared_ptr<TrivialReceiver> receiver =
            make_shared<TrivialReceiver>(event_loop);

    SECTION("Connect/Disconnect")
    {
        event_loop->Start();
        std::thread thread0( [event_loop](){ event_loop->Run(); });

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
        event_loop->Start();
        std::thread thread1( [event_loop](){ event_loop->Run(); });

        ok = false;
        signal_check.Emit(&ok);
        signal_stop.Emit(event_loop.get());
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
        event_loop->Start();
        std::thread thread( [event_loop](){ event_loop->Run(); });

        Signal<bool*> signal_check;
        Id cid0;

        {
            shared_ptr<TrivialReceiver> temp_receiver =
                    make_shared<TrivialReceiver>(event_loop);

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
        event_loop->Start();
        std::thread thread0( [event_loop](){ event_loop->Run(); });

        // signals
        Signal<> signal_count;
        Signal<EventLoop*> signal_stop;

        // r0
        shared_ptr<TrivialReceiver> r0 =
                make_shared<TrivialReceiver>(event_loop);

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
        shared_ptr<TrivialReceiver> r1 = make_shared<TrivialReceiver>(event_loop);
        shared_ptr<TrivialReceiver> r2 = make_shared<TrivialReceiver>(event_loop);
        shared_ptr<TrivialReceiver> r3 = make_shared<TrivialReceiver>(event_loop);

        // zero count
        r0->invoke_count = 0;
        r1->invoke_count = 0;
        r2->invoke_count = 0;
        r3->invoke_count = 0;

        size_t const one_many_count=100;

        // restart event loop
        event_loop->Start();
        std::thread thread1( [event_loop](){ event_loop->Run(); });

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
}

// ============================================================= //
// ============================================================= //

class WakeupReceiver : public Object
{
public:
    WakeupReceiver(shared_ptr<EventLoop> event_loop) :
        Object(event_loop),
        m_wakeup_count(0),
        m_wakeup_limit(0),
        m_waiting(false)
    {
       // empty
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

    SECTION("single timer: ") {

        shared_ptr<EventLoop> event_loop =
                make_shared<EventLoop>();

        event_loop->Start();
        std::thread thread( [event_loop](){ event_loop->Run(); });

        shared_ptr<Timer> timer =
                make_shared<Timer>(event_loop);

        shared_ptr<WakeupReceiver> receiver =
                make_shared<WakeupReceiver>(event_loop);

        timer->SignalTimeout.Connect(
                    receiver,
                    &WakeupReceiver::OnWakeup);

        SECTION("single shot / sequential: ") {
            // single shot
            std::chrono::time_point<std::chrono::steady_clock> start,end;
            start = std::chrono::steady_clock::now();

            timer->Start(std::chrono::milliseconds(50),false);
            receiver->Prepare(1); // wait for 1 timeout signal
            receiver->Block();

            end = std::chrono::steady_clock::now();
            std::chrono::milliseconds interval_ms =
                    std::chrono::duration_cast<
                        std::chrono::milliseconds
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
            timer->Start(std::chrono::milliseconds(50),false);
            timer->Start(std::chrono::milliseconds(60),false);
            timer->Start(std::chrono::milliseconds(70),false);
            receiver->Block();

            end = std::chrono::steady_clock::now();
            interval_ms =
                    std::chrono::duration_cast<
                        std::chrono::milliseconds
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
            timer->Start(std::chrono::milliseconds(33),true);
            receiver->Block();

            end = std::chrono::steady_clock::now();
            std::chrono::milliseconds interval_ms =
                    std::chrono::duration_cast<
                        std::chrono::milliseconds
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
    }
}

// ============================================================= //
// ============================================================= //

class TrivialApplication : public Application
{
public:
    TrivialApplication() :
        m_ret_val(0),
        m_keep_running(false)
    {

    }

    ~TrivialApplication()
    {

    }

    sint Run()
    {
        this->GetEventLoop()->Start();
        m_keep_running = true;

        while(m_keep_running) {
            this->GetEventLoop()->ProcessEvents();

            std::this_thread::sleep_for(
                std::chrono::milliseconds(16));
        }

        return m_ret_val;
    }

private:
    void quit(sint ret_val)
    {
        m_keep_running = false;
        this->GetEventLoop()->Stop();
        m_ret_val = ret_val;
    }

    sint m_ret_val;
    bool m_keep_running;
};

// ============================================================= //

class CleanupObject : public Object
{
public:
    CleanupObject(shared_ptr<EventLoop> event_loop,
                  std::atomic<uint> * i) :
        Object(event_loop),
        m_i(i)
    {

    }

    ~CleanupObject()
    {

    }

    void OnCleanup()
    {
        // clean up some stuff
        std::atomic<uint> &i = (*m_i);
        i--;

        SignalFinishedCleanup.Emit(this->GetId());
    }

    Signal<Id> SignalFinishedCleanup;

private:
    std::atomic<uint> * m_i;
};

// ============================================================= //

TEST_CASE("ks::Application","[application]") {

    shared_ptr<Application> app =
            make_shared<TrivialApplication>();

    SECTION("Test cleanup") {

        std::atomic<uint> i(4);

        // cleanup objects in app event_loop
        shared_ptr<CleanupObject> r0 =
                make_shared<CleanupObject>(app->GetEventLoop(),&i);

        shared_ptr<CleanupObject> r1 =
                make_shared<CleanupObject>(app->GetEventLoop(),&i);

        // cleanup objects in alt event_loop
        shared_ptr<EventLoop> event_loop = make_shared<EventLoop>();
        event_loop->Start();
        std::thread thread( [event_loop](){ event_loop->Run(); });

        shared_ptr<CleanupObject> r2 =
                make_shared<CleanupObject>(event_loop,&i);

        shared_ptr<CleanupObject> r3 =
                make_shared<CleanupObject>(event_loop,&i);

        app->AddCleanupRequest(r0);
        app->AddCleanupRequest(r1);
        app->AddCleanupRequest(r2);
        app->AddCleanupRequest(r3);

        // connect
        app->SignalStartCleanup.Connect(
                    r0,&CleanupObject::OnCleanup);

        app->SignalStartCleanup.Connect(
                    r1,&CleanupObject::OnCleanup);

        app->SignalStartCleanup.Connect(
                    r2,&CleanupObject::OnCleanup);

        app->SignalStartCleanup.Connect(
                    r3,&CleanupObject::OnCleanup);


        r0->SignalFinishedCleanup.Connect(
                    app,&Application::OnFinishedCleanup);

        r1->SignalFinishedCleanup.Connect(
                    app,&Application::OnFinishedCleanup);

        r2->SignalFinishedCleanup.Connect(
                    app,&Application::OnFinishedCleanup);

        r3->SignalFinishedCleanup.Connect(
                    app,&Application::OnFinishedCleanup);

        app->SignalStartCleanup.Emit();
        app->Run();

        event_loop->Stop();
        thread.join();

        REQUIRE(i==0);
    }
}

