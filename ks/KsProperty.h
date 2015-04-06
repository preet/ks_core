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

#ifndef KS_PROPERTY_H
#define KS_PROPERTY_H

#include <vector>
#include <algorithm>
#include <ks/KsGlobal.h>

namespace ks
{
    // ============================================================= //

    // ref: http://woboq.com/blog/property-bindings-in-cpp.html

    class PropertyBase
    {
    public:
        PropertyBase();
        virtual ~PropertyBase();

        PropertyBase(PropertyBase const &other) = delete;
        PropertyBase(PropertyBase &&other) = delete;
        PropertyBase & operator = (PropertyBase const &) = delete;
        PropertyBase & operator = (PropertyBase &&) = delete;

        std::vector<PropertyBase*> const & GetInputs() const;
        std::vector<PropertyBase*> const & GetOutputs() const;

        static void RemoveProperty(std::vector<PropertyBase*> &list_props,
                                   PropertyBase* rem_prop);

        static void TopologicalSort(PropertyBase* property,
                                    std::vector<PropertyBase*> &list_sorted_props);

    protected:
        void captureAsInput();
        void evaluateOutputs();
        void clearInputs();
        void clearOutputs();
        void setCurrent(PropertyBase* prop);
        virtual void evaluate() = 0;

    private:
        void registerInput(PropertyBase* input_prop);
        virtual void resetBinding() = 0;

        std::vector<PropertyBase*> m_list_inputs;
        std::vector<PropertyBase*> m_list_outputs;

        // helper for topological sort:
        // 0: unvisited
        // 1: visited
        // 2: finished
        u8 m_vx_state;
    };

    // ============================================================= //

    template<typename T>
    class Property final : public PropertyBase
    {
    public:
        using BindingFn = std::function<T()>;
        using NotifierFn = std::function<void(T const &)>;

        Property()
        {

        }

        Property(T value,
                 NotifierFn notifier=NotifierFn{}) :
            m_value(value),
            m_binding_init(false),
            m_notifier(notifier)
        {

        }

        Property(BindingFn binding,
                 NotifierFn notifier=NotifierFn{}) :
            m_binding(binding),
            m_binding_init(false),
            m_notifier(notifier)
        {
            evaluate();
        }

        ~Property()
        {
            clearInputs();
            clearOutputs();
        }

        T const &Get()
        {
            captureAsInput();
            return m_value;
        }

        bool GetBindingValid() const
        {
            return bool(m_binding);
        }

        void Assign(T value)
        {
            // Clear inputs and binding since
            // assignment breaks bindings
            clearInputs();

            // Save new value
            m_value = std::move(value);

            // Notify
            if(m_notifier) {
                m_notifier(m_value);
            }

            evaluateOutputs();
        }

        void Bind(BindingFn binding)
        {
            clearInputs();

            m_binding = std::move(binding);
            evaluate(); // captures new inputs

            evaluateOutputs();
        }

        void evaluate()
        {
            if(m_binding) {
                if(m_binding_init) {
                    m_value = m_binding();
                }
                else {
                    // capture inputs for this property
                    setCurrent(this);
                    m_value = m_binding();
                    setCurrent(nullptr);
                    m_binding_init = true;
                }
            }

            // Notify
            if(m_notifier) {
                m_notifier(m_value);
            }
        }

    private:
        void resetBinding()
        {
            m_binding = BindingFn{};
            m_binding_init = false;
        }

        T m_value;
        BindingFn m_binding;
        bool m_binding_init;
        NotifierFn m_notifier;
    };
    
    // ============================================================= //
}

#endif // KS_PROPERTY_H
