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

#ifndef KS_APPLICATION_H
#define KS_APPLICATION_H

#include <ks/KsObject.h>
#include <ks/KsSignal.h>

namespace ks
{
    class Application : public Object
    {
        friend class ObjectBuilder;
        typedef Object base_type;

    public:
        ~Application();

        // TODO get rid of this cleanup stuff

        // * adds @obj to a list of objects that the application
        //   will wait on for a 'finished clean up' signal before
        //   quitting
        // * @obj should connect to OnFinishedCleanup
        // * this function is thread safe
        void AddCleanupRequest(shared_ptr<Object> const &obj);

        // * starts the application event loop
        virtual sint Run() = 0;


        // Slots

        // * should be notified by objects that connect to
        //   ks::Application::SignalStartCleanup and have
        //   finished their clean up
        // * removes the sender (@object_id) from the list
        //   of objects the application is waiting on to
        //   be cleaned up
        // * calls quit() to exit the application once no
        //   more finished clean up signals are pending
        void OnFinishedCleanup(Id object_id);


        // Signals

        // * emitted when the application has been
        //   requested to quit
        Signal<> SignalStartCleanup;

    protected:
        // * must be constructed from the main thread!
        Application();

        // * should be called to initiate a clean exit
        // * will call SignalStartCleanup and quit()
        //   as necessary
        void startCleanup();

        // * breaks out of the event loop and sets the
        //   return value returned by Run() to @ret_val
        virtual void quit(sint ret_val) = 0;

        std::mutex m_mutex_list_cleanup_objs;
        std::vector<shared_ptr<Object>> m_list_cleanup_objs;

    private:
        void init();

        std::thread::id m_sys_thread_id;
    };

} // ks

#endif // KS_APPLICATION_H
