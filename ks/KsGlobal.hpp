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

#ifndef KS_GLOBAL_HPP
#define KS_GLOBAL_HPP

#include <cstdint>
#include <memory>
#include <sstream>
#include <type_traits>
#include <utility>
#include <iomanip>
#include <chrono>

/// \namespace ks
/// * The main namespace for lib ks
/// * Contains helper utils and convenience types in addition
///   to core classes
/// * Of special note is that the ks namespace includes:
///	\code
///	using std::shared_ptr;
///	using std::unique_ptr;
///	using std::weak_ptr;
///	using std::make_shared;
/// \endcode
/// * This allows the use of <memory> types without bringing
///   in the std:: namespace. Code in the ks namespace will
///   often look like:
///	\code
///	namespace ks {
///		shared_ptr<EventLoop> ev_loop = make_shared<EventLoop>();
///     unique_ptr<Thing> thing = make_unique<Thing>();
///	}
/// \endcode
namespace ks
{
    // Convenience typedefs
    using uint = unsigned int;
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using sint = int;
    using s8 = std::int8_t;
    using s16 = std::int16_t;
    using s32 = std::int32_t;
    using s64 = std::int64_t;

    /// * The standard data type for Ids in ks is a 64-bit
    ///   unsigned integer. Ids usually aren't recycled and
    ///   are simply incremented monotonically
    using Id = u64;

    using std::shared_ptr;
    using std::unique_ptr;
    using std::weak_ptr;

    // Note: each of the predefined duration types
    // covers a range of at least ±292 years
    using Microseconds = std::chrono::microseconds;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;
    using Minutes = std::chrono::minutes;
    using Hours = std::chrono::hours;

    using TimePoint =
        std::chrono::time_point<
            std::chrono::high_resolution_clock>;

    /// \cond HIDE_DOCS
    // make_unique for pre c++14 compilers
    // http://stackoverflow.com/questions/7038357/make-unique-and-perfect-forwarding
    template <typename T, typename... Args>
    std::unique_ptr<T> make_unique_helper(std::false_type, Args&&... args) {
      return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

    template <typename T, typename... Args>
    std::unique_ptr<T> make_unique_helper(std::true_type, Args&&... args) {
       static_assert(std::extent<T>::value == 0,
           "make_unique<T[N]>() is forbidden, please use make_unique<T[]>().");

       typedef typename std::remove_extent<T>::type U;
       return std::unique_ptr<T>(new U[sizeof...(Args)]{std::forward<Args>(args)...});
    }
    /// \endcond

    /// * Equivalent to std::make_unique
    /// * Included because make_unique isn't part of the C++11 standard
    template <typename T, typename... Args>
    unique_ptr<T> make_unique(Args&&... args) {
       return make_unique_helper<T>(std::is_array<T>(), std::forward<Args>(args)...);
    }

    // since for now we have an unqualified (ie no std::)
    // make_unique function, having make_shared the same
    // is more syntactically consistent
    using std::make_shared;

    /// * Converts common types to std::string
    /// * Included instead of using std::to_string because the latter
    ///   is missing on Android
    template<typename T>
    std::string ToString(T const &val)
    {
        std::ostringstream oss;
        oss << val;
        return oss.str();
    }

    template<typename T>
    std::string ToStringFormat(T const &val,
                               uint precision,
                               uint width,
                               char fill)
    {
        std::ostringstream oss;
        oss << std::fixed
            << std::setw(width)
            << std::setfill(fill)
            << std::setprecision(precision) << val;
        return oss.str();
    }

    template<typename T>
    T CalcDuration(TimePoint const &before,TimePoint const &after)
    {
        return std::chrono::duration_cast<T>(after-before);
    }

} // ks

#endif // KS_GLOBAL_HPP
