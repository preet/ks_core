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
        PropertyBase(std::string name="");
        virtual ~PropertyBase();

        PropertyBase(PropertyBase const &other) = delete;
        PropertyBase(PropertyBase &&other) = delete;
        PropertyBase & operator = (PropertyBase const &) = delete;
        PropertyBase & operator = (PropertyBase &&) = delete;

        std::string const & GetName() const;
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

        std::string m_name;
        bool m_capture_failed;

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

    struct ReadWrite {
        static constexpr bool access=true;
    };

    struct ReadOnly {
        static constexpr bool access=false;
    };

    template<typename T>
    struct PropertyInit final
    {
        using BindingFn = std::function<T()>;
        using NotifierFn = std::function<void(T const &)>;

        PropertyInit(T value) : has_value(true),value(value) {}
        PropertyInit(std::string name, T value) : name(name),has_value(true),value(value) {}

        PropertyInit(BindingFn b) : has_value(false),binding(b) {}
        PropertyInit(std::string name, BindingFn b) : name(name),has_value(false),binding(b) {}

        PropertyInit(BindingFn b, NotifierFn n) : has_value(false),binding(b),notifier(n) {}
        PropertyInit(std::string name, BindingFn b, NotifierFn n) : name(name),has_value(false),binding(b),notifier(n) {}

        std::string name;
        bool has_value;
        T value;
        BindingFn binding;
        NotifierFn notifier;
    };

    template<typename T, typename AccessType=ReadWrite>
    class Property final : public PropertyBase
    {
    public:
        using BindingFn = std::function<T()>;
        using NotifierFn = std::function<void(T const &)>;

        Property() :
            PropertyBase("")
        {

        }

        Property(T value,
                 NotifierFn notifier=NotifierFn{}) :
            PropertyBase(""),
            m_value(value),
            m_binding_init(false),
            m_notifier(notifier)
        {

        }

        Property(std::string name,
                 T value,
                 NotifierFn notifier=NotifierFn{}) :
            PropertyBase(std::move(name)),
            m_value(value),
            m_binding_init(false),
            m_notifier(notifier)
        {

        }

        Property(BindingFn binding,
                 NotifierFn notifier=NotifierFn{}) :
            PropertyBase(""),
            m_binding(binding),
            m_binding_init(false),
            m_notifier(notifier)
        {
            evaluate();
        }

        Property(std::string name,
                 BindingFn binding,
                 NotifierFn notifier=NotifierFn{}) :
            PropertyBase(std::move(name)),
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
            static_assert(AccessType::access,
                          "Cannot call Assign() on "
                          "a read-only property");

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
            static_assert(AccessType::access,
                          "Cannot call Bind() on "
                          "a read-only property");

            clearInputs(); // resets binding!

            m_binding = std::move(binding);

            evaluate(); // captures new inputs

            evaluateOutputs();
        }

        void SetName(std::string name)
        {
            m_name = std::move(name);
        }

        void SetNotifier(NotifierFn notifier)
        {
            m_notifier = std::move(notifier);
        }

        void SetAll(PropertyInit<T> set_all)
        {
            SetName(set_all.name);
            SetNotifier(set_all.notifier);
            if(set_all.has_value) {
                Assign(set_all.value);
            }
            else {
                Bind(set_all.binding);
            }
        }

        void evaluate()
        {
            if(m_binding) {
                if(m_binding_init) {
                    m_value = m_binding();

                    if(m_notifier) {
                        m_notifier(m_value);
                    }
                }
                else {
                    // capture inputs for this property
                    m_capture_failed = false;
                    setCurrent(this);
                    m_value = m_binding();
                    setCurrent(nullptr);

                    if(!m_capture_failed) {
                        m_binding_init = true;

                        if(m_notifier) {
                            m_notifier(m_value);
                        }
                    }
                    else {
                        resetBinding();
                    }
                }
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
