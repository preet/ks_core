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
    class Object;

    // ============================================================= //

    /// * ObjectBuilder is an implementation detail class that helps
    ///   to construct and initialize Objects
    /// * Any classes that are derived from Object must friend this
    ///   class in order to be properly constructed (ie. friend class
    ///   ObjectBuilder)
    /// * Methods in ObjectBuilder should never be called directly
    ///   from user code (use ks::make_object instead)
    class ObjectBuilder
    {
    public:
        /// * Underlying implementation of ks::make_object
        /// * Constructs the Object-derived class and calls init()
        ///   on all parent classes in the inheritance chain
        template<typename T, typename... Args>
        static std::shared_ptr<T> make_object(Args&&... args)
        {
            static_assert(std::is_base_of<Object,T>::value,
                          "ks::make_object: Type must be a "
                          "descendant ks::Object");

            // This Access class grants std::make_shared access to call
            // T's constructor which should be private or protected.

            // A struct or class declared within a function definition
            // has the same access as the enclosing function. So if we
            // grant this function friendship from another class, then
            // Access will have the same access level

            // ref: http://stackoverflow.com/questions/28733451/...
            // how-is-friendship-conferred-for-a-class-defined-within-a-friend-function

            struct Access : public T {
                Access(Args&&... args) :
                    T(std::forward<Args>(args)...) {
                    // empty
                }
            };

            // call T's constructor
            std::shared_ptr<T> ob =
                    std::make_shared<Access>(
                        std::forward<Args>(args)...);

            // call T::init
            init_object(ob.get());

            return ob;
        }

    private:
        /// * Recursively calls init() on the inheritance hierarchy
        ///   of an Object-derived class. For example, a chain like
        ///   Object <- Animal <- Leopard would call init() in the
        ///   following order:
        ///    -# Object::init()
        ///    -# Animal::init()
        ///    -# Leopard::init()
        /// * init_object relies on the Object-derived class correctly
        ///   specifying its immediate parent in the typedef base_type.
        ///   See the Object class for more information.
        template<typename T>
        static void init_object(T * thing)
        {
            if(typeid(typename T::base_type) != typeid(T)) {
                init_object<typename T::base_type>(thing);
            }
            thing->T::init();
        }
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
    ///   related setup can occur in Object::init()
    template<typename T, typename... Args>
    static std::shared_ptr<T> make_object(Args&&...args)
    {
        return ObjectBuilder::make_object<T>(std::forward<Args>(args)...);
    }

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
    ///    // All classes that inherit ks::Object must declare
    ///    // themselves as a friend of ObjectBuilder. This is
    ///    // required by ks::make_object to construct this object.
    ///    friend class ks::ObjectBuilder;
    ///
    ///    // All classes that inherit ks::Object must define
    ///    // base_type to be the immediate base class of the
    ///    // class. This is required by ks::make_object to init
    ///    // the object.
    ///    typedef Object base_type;
    ///
    ///    // Constructors should be marked private or protected
    ///    // and not public to ensure only ks::make_object can
    ///    // be used for creation.
    ///    private:
    ///       Derived(shared_ptr<ks::EventLoop> const &evloop, /* other args */) :
    ///          ks::Object(evloop) {}
    ///
    ///    // Each Object-derived class should declare a private
    ///    // init() method where any initialization that requires
    ///    // access to 'shared_from_this' (ie for connecting to
    ///    // signals) can occur
    ///    private:
    ///       void init() {}
    /// };
    ///
    /// // A more succint example
    /// class DerivedAgain : public Derived
    /// {
    ///    friend class ks::ObjectBuilder;
    ///    typedef Derived base_type; // note this isn't ks::Object, but Derived
    ///
    ///    private:
    ///       DerivedAgain(shared_ptr<ks::EventLoop> const &evloop, /* other args */) :
    ///          Derived(evloop) {}
    ///
    ///       void init() {}
    /// };
    /// \endcode
    /// * Using make_object:
    /// \code
    /// // Create an event loop
    /// ks::shared_ptr<ks::EventLoop> ev_loop =
    ///    ks::make_shared<ks::EventLoop>();
    ///
    /// // Create the object
    /// ks::shared_ptr<DerivedAgain> =
    ///    ks::make_object<DerivedAgain>(ev_loop,...);
    /// \endcode
    class Object : public std::enable_shared_from_this<Object>
    {
        friend class ObjectBuilder;
        typedef Object base_type;

    public:
        virtual ~Object();

        /// * Returns this Object's unique Id
        Id GetId() const;

        /// * Returns this Object's EventLoop
        shared_ptr<EventLoop> const & GetEventLoop() const;

    protected:
        /// * Creates the object
        /// \param event_loop
        ///     The event loop that will handle events,
        ///     for this object including slot callbacks
        Object(shared_ptr<EventLoop> const &event_loop);

    private:
        Object(Object const &other) = delete;
        Object(Object &&other) = delete;
        Object & operator = (Object const &) = delete;
        Object & operator = (Object &&) = delete;

        void init();

        Id const m_id;

        shared_ptr<EventLoop> m_event_loop;

        static std::mutex s_id_mutex;
        static Id s_id_counter;
        static Id genId();
    };

    // ============================================================= //

} // ks

#endif // KS_OBJECT_H
