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
#include <typeindex>

#include <ks/KsGlobal.h>
#include <ks/KsEventLoop.h>

namespace ks
{
    class Event;
    class Object;


    // ============================================================= //

    /// * The base object class for slots. Any class wishing to use
    ///   slots to connect to signals must inherit Object
    /// * All Object-derived classes must be created with make_object
    ///   and should not be constructed otherwise, or moved or copied
    /// * All Objects are created as shared_ptrs to facilitate
    ///   lifetime tracking. This allows Signals to safely remove
    ///   Object references when they are destroyed without the need to
    ///   explicitly manage connections within the Object
    /// * To reiterate, once Objects are destroyed any external
    ///   Signal connections will be automatically cleaned up
    /// * Each Object has a non-recycled unique Id assigned to it
    ///   during construction
    /// * An example on how to inherit from Object and use make_object:
    /// \code
    /// #include <ks/KsObject.h>
    ///
    /// class Derived : public ks::Object
    /// {
    /// public:
    ///    // All classes that inherit ks::Object must define
    ///    // base_type to be the immediate base class of the
    ///    // class. This is required by ks::make_object to init
    ///    // the object.
    ///    using base_type = Object; // or the immediate Base class
    ///
    ///    // Constructors must have *Object::Key const &* as their
    ///    // first parameter. This enforces creation of all Objects
    ///    // and any derivatives through ks::make_object
    ///       Derived(Key const &key,
    ///               shared_ptr<ks::EventLoop> const &evloop,
    ///               /* other args */) :
    ///          ks::Object(key,evloop) {}
    ///
    ///    // Each Object-derived class should declare a static Init()
    ///    // member method with the exact signature shown
    ///    // where any initialization that requires access to
    ///    // 'shared_from_this' (ie for connecting to signals) can occur
    ///       static void Init(Object::Key const &,shared_ptr<Derived>) {}
    /// };
    ///
    /// // A more succint example
    /// class DerivedAgain : public Derived
    /// {
    /// public:
    ///
    ///    using base_type = Derived; // note this isn't ks::Object, but Derived
    ///
    ///    DerivedAgain(Key const & key,
    ///                 shared_ptr<ks::EventLoop> const &evloop, /* other args */) :
    ///         Derived(key,evloop) {}
    ///
    ///    static void Init(Key const &,shared_ptr<DerivedAgain>) {}
    /// };
    /// \endcode
    /// * Using make_object:
    /// \code
    /// // Create an event loop
    /// ks::shared_ptr<ks::EventLoop> ev_loop =
    ///    ks::make_shared<ks::EventLoop>();
    ///
    /// // Create the object
    /// // Note that Object::Key is not passed to the params;
    /// // its the responsibility of make_object to add this
    /// ks::shared_ptr<DerivedAgain> =
    ///    ks::make_object<DerivedAgain>(ev_loop,...);
    /// \endcode
    class Object : public std::enable_shared_from_this<Object>
    {
    public:
        using base_type = Object;

        // TODO desc
        class Key {
            template<typename T, typename... Args>
            friend std::shared_ptr<T> make_object(Args&& ...args);

            Key() {} // private constructor
        };

        /// * Creates the object
        /// \param key
        ///     The construction/init key needed to create this
        ///     object (pass-key idiom). Enforces creation through
        ///     ks::make_object(...)
        /// \param event_loop
        ///     The event loop that will handle events,
        ///     for this object including slot callbacks
        Object(Key const &key,
               shared_ptr<EventLoop> const &event_loop);

        void Init(Key const &key,
                  shared_ptr<Object> const &object);

        virtual ~Object();

        /// * Returns this Object's unique Id
        Id GetId() const;

        /// * Returns this Object's EventLoop
        shared_ptr<EventLoop> const & GetEventLoop() const;

    private:
        Object(Object const &other) = delete;
        Object(Object &&other) = delete;
        Object & operator = (Object const &) = delete;
        Object & operator = (Object &&) = delete;

        Id const m_id;

        shared_ptr<EventLoop> m_event_loop;

        static std::mutex s_id_mutex;
        static Id s_id_counter;
        static Id genId();
    };

    // ============================================================= //

    /// * make_object **must** be called to create any
    ///   ks::Object-derived object
    /// * Details: make_object is required because:
    ///   - Object-derived classes must be wrapped in a shared_ptr to
    ///     connect to any Signals
    ///   - shared_ptrs to an object cannot be used within its own
    ///     constructor
    /// * As a result, make_object does two phase construction where
    ///   normal construction can occur in the constructor, and Signal
    ///   related setup can occur in Object::Init()
    /// * Note that the args param does not contain Object::Key, as
    ///   this is inserted by make_object
    template<typename T, typename... Args>
    std::shared_ptr<T> make_object(Args&&...args)
    {
        Object::Key key; // creation key

        // TODO
        // Consider using new instead of make_shared because
        // of an issue where memory isn't freed as long as
        // weak_ptrs exist:
        // https://lanzkron.wordpress.com/2012/04/22/...
        // ...make_shared-almost-a-silver-bullet/

        // call T's constructor
        shared_ptr<T> object =
                std::make_shared<T>(
                    key,std::forward<Args>(args)...);

        // initialize T
        init_object(key,object);

        return object;
    }


    // * Uses sfinae to ensure that a type T has a member function
    //   named T::Init that matches a specific signature
    // * Returns std::false_type if type T doesn't contain a member
    //   function named Init or if the signature doesn't match
    struct check_init_signature
    {
        template<typename T, typename X=decltype(&T::Init)>
        static std::is_same<void(T::*)(Object::Key const &,shared_ptr<T> const &), X> test(int);

        template<typename...>
        static std::false_type test(...);
    };

    template<typename T>
    struct has_init_signature : decltype(check_init_signature::test<T>(0)) {};


    /// * Recursively calls Init() on the inheritance hierarchy
    ///   of an Object-derived class. For example, a chain like
    ///   Object <- Animal <- Leopard would call Init() in the
    ///   following order:
    ///    -# Object::Init()
    ///    -# Animal::Init()
    ///    -# Leopard::Init()
    /// * init_object relies on the Object-derived class correctly
    ///   specifying its immediate parent in the typedef base_type.
    ///   See the Object class for more information.
    template<typename Derived>
    void init_object(Object::Key const &key,
                     std::shared_ptr<Derived> const &d)
    {
        // Ensure that a function with signature:
        // void Derived::Init(Object::Key const &,shared_ptr<Derived>) exists
        static_assert(has_init_signature<Derived>::value,
                      "ks::init_object: Any class T that inherits ks::Object "
                      "must have an Init member method with the exact signature: "
                      "void Init(Object::Key const &,shared_ptr<T> const &)");

        // Recursively call init_object on the inheritance hierarchy,
        // stopping when ks::Object is reached
        using Base = typename Derived::base_type;
        if(!std::is_same<Base,Derived>::value) {
            init_object<Base>(key,d);
        }
        d->Init(key,d);
    }


    // ============================================================= //

} // ks

#endif // KS_OBJECT_H
