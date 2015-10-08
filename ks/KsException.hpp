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

#ifndef KS_EXCEPTION_HPP
#define KS_EXCEPTION_HPP

#include <exception>
#include <ks/KsLog.hpp>

namespace ks
{
    class Exception : public std::exception
    {
    public:
        using ErrorLevel = ks::Log::Logger::Level;

        Exception();
        Exception(ErrorLevel err_lvl,std::string msg,bool stack_trace=false);
        virtual ~Exception();

        virtual const char* what() const noexcept;

    protected:
        static std::vector<std::string> const m_lkup_err_lvl;
        std::string m_msg;
    };
}

#endif // KS_EXCEPTION_HPP
