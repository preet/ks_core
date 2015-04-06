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

#include <pthread.h>

#include <ks/KsProperty.h>
#include <ks/KsLog.h>

namespace ks
{
    // ============================================================= //
    // ============================================================= //

    namespace {

        // ============================================================= //

        // TODO desc
        // Work around for unreliable thread_local support in c++11
        // refs:
        // https://github.com/ChromiumWebApps/chromium/blob/master...
        // .../base/threading/thread_local.h
        // .../base/threading/thread_local_posix.cc
        template<typename T>
        class ThreadLocalPtr
        {
            std::string const m_log_prefix="ks::Property::ThreadLocalPtr: ";

        public:
            ThreadLocalPtr()
            {
                // create key (no destructor)
                int error = pthread_key_create(&m_key,NULL);

                if(error != 0) {
                    LOG.Fatal() << m_log_prefix << "Could not create key: ";
                    if(error == EAGAIN) {
                        // TODO key counter
                        LOG.Fatal() << m_log_prefix << "key limit reached";
                    }
                    else if(error == ENOMEM) {
                        LOG.Fatal() << m_log_prefix << "insufficient memory";
                    }
                    std::abort();
                }

                // init to null
                Set(NULL);
            }

            ~ThreadLocalPtr()
            {
                // delete key
                int error = pthread_key_delete(m_key);

                if(error != 0) {
                    LOG.Fatal() << m_log_prefix << "Could not delete key: ";
                    if(error == EINVAL) {
                        LOG.Fatal() << m_log_prefix << "key value is invalid";
                    }
                    std::abort();
                }
            }

            T* Get()
            {
                return static_cast<T*>(pthread_getspecific(m_key));
            }

            void Set(T* ptr)
            {
                int error = pthread_setspecific(m_key,ptr);

                if(error != 0) {
                    LOG.Fatal() << m_log_prefix << "Could not set key: ";
                    if(error == EINVAL) {
                        LOG.Fatal() << m_log_prefix << "key value is invalid";
                    }
                    else {
                        LOG.Fatal() << m_log_prefix << "insufficient memory";
                    }
                    std::abort();
                }
            }

        private:
            pthread_key_t m_key;
        };

        // ============================================================= //

        // * This will be initialized once since its global and set
        //   to null in its constructor
        // * Calls to ->Set() will set a thread local pointer
        // * Calls to ->Get() will get the thread local pointer
        // * The property that is currently being assigned a binding
        //   in the calling thread
        // * Used to temporarily store a property to implicitly capture
        //   its inputs as its binding is evaluated for the first time
        // * PropertyBase* may be null
        ThreadLocalPtr<PropertyBase> g_current_prop;
    }

    // ============================================================= //
    // ============================================================= //

    PropertyBase::PropertyBase() :
        m_vx_state(false)
    {
        m_list_inputs.reserve(8);
        m_list_outputs.reserve(8);
    }

    PropertyBase::~PropertyBase()
    {

    }

    std::vector<PropertyBase*> const & PropertyBase::GetInputs() const
    {
        return m_list_inputs;
    }
    std::vector<PropertyBase*> const & PropertyBase::GetOutputs() const
    {
        return m_list_outputs;
    }

    void PropertyBase::RemoveProperty(std::vector<PropertyBase*> &list_props,
                                      PropertyBase* rem_prop)
    {
        list_props.erase(
                    std::remove_if(
                        list_props.begin(),
                        list_props.end(),
                        [rem_prop](PropertyBase* prop) {
                            return (prop == rem_prop);
                        }),
                    list_props.end());
    }

    void PropertyBase::TopologicalSort(PropertyBase* property,
                                       std::vector<PropertyBase*> &list_rev_sorted_props)
    {
        // mark visited
        property->m_vx_state = 1;

        for(PropertyBase* output : property->m_list_outputs) {
            // unvisited, most likely case
            if(output->m_vx_state==0) {
                TopologicalSort(output,list_rev_sorted_props);
            }
            // already visited, indiciates a cycle
            else if(output->m_vx_state==1) {
                LOG.Warn() << "Property: Cycle detected";
                return;
            }
            // else: output vertex is finished,
            // no need to do anything
        }

        // mark traversal finished for this vertex
        property->m_vx_state = 2;
        list_rev_sorted_props.push_back(property);
    }

    using sint = int32_t;

    void PropertyBase::evaluateOutputs()
    {
        // We do a topological sort (see ref1) on all of
        // the outputs and evaluate the result ordering.
        // This allows us to:
        // * Avoid redundant/repeat evaluations
        // * Avoid 'glitches' (see ref2)
        // * Detect cycles

        // ref1: http://www.cs.cornell.edu/courses/cs2112/2012sp/lectures/lec24/lec24-12sp.html

        // ref2: "A Survey on Reactive Programming":
        //       http://soft.vub.ac.be/Publications/2012/vub-soft-tr-12-13.pdf

        std::vector<PropertyBase*> list_rev_sorted_props;
        list_rev_sorted_props.reserve(8);

        TopologicalSort(this,list_rev_sorted_props);

        // Assume that this property has already been evaluated
        // so ignore the last entry in the list (which represents
        // this property)

        // The list returned from the topological sort is
        // reversed so we iterate backwards
        for(auto it = std::next(list_rev_sorted_props.rbegin());
            it != list_rev_sorted_props.rend(); ++it)
        {
            PropertyBase* prop = (*it);
            prop->evaluate();
            prop->m_vx_state = 0; // reset state
        }

        // reset this property's state
        this->m_vx_state = 0;
    }

    void PropertyBase::captureAsInput()
    {
        PropertyBase* current = g_current_prop.Get();
        if(current) {
            // Add this property as an input
            current->registerInput(this);
        }
    }

    void PropertyBase::clearInputs()
    {
        // Remove this property from inputs
        for(PropertyBase* input : m_list_inputs) {
            input->RemoveProperty(input->m_list_outputs,this);
        }

        resetBinding();

        m_list_inputs.clear();
    }

    void PropertyBase::clearOutputs()
    {
        // Remove this property from outputs
        for(PropertyBase* output : m_list_outputs) {
            output->RemoveProperty(output->m_list_inputs,this);
            output->resetBinding();
        }

        m_list_outputs.clear();
    }

    void PropertyBase::setCurrent(PropertyBase* prop)
    {
        g_current_prop.Set(prop);
    }

    void PropertyBase::registerInput(PropertyBase* input_prop)
    {
        // Add this property to the input's outputs
        // if it doesn't exist
        {
            auto it = std::find_if(
                        input_prop->m_list_outputs.begin(),
                        input_prop->m_list_outputs.end(),
                        [this](PropertyBase* prop) {
                            return(prop==this);
                        });

            if(it == input_prop->m_list_outputs.end()) {
                input_prop->m_list_outputs.push_back(this);
            }
        }


        // Add input if it doesn't exist
        {
            auto it = std::find_if(
                        m_list_inputs.begin(),
                        m_list_inputs.end(),
                        [input_prop](PropertyBase* prop) {
                            return(prop==input_prop);
                        });

            if(it == m_list_inputs.end()) {
                m_list_inputs.push_back(input_prop);
            }
        }
    }

    // ============================================================= //
    // ============================================================= //
}
