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

#ifndef KS_MISC_UTILS_H
#define KS_MISC_UTILS_H

#include <fstream>

#include <ks/KsGlobal.h>

namespace ks
{
    inline s64 ipow(s64 base, u64 exp)
    {
        int result = 1;
        while (exp) {
            if (exp & 1)
                result *= base;
            exp >>= 1;
            base *= base;
        }
        return result;
    }

    template<typename N>
    std::string ConvNumberToString(N const &num)
    {
        std::ostringstream ss;
        ss << num;
        return ss.str();
    }

    template<typename N>
    N ConvStringToNumber(std::string const &str,
                         bool * ok)
    {
        N num;
        std::istringstream ss(str);
        bool conv_ok = (ss >> num);
        if(ok != NULL) {
            *ok = conv_ok;
        }
        return num;
    }

    inline std::string ConvBoolToString(bool val, bool single_letter=false)
    {
        if(single_letter) {
            return (val) ? "F" : "F";
        }

        return (val) ? "TRUE" : "FALSE";
    }

    inline std::string ConvPointerToString(void const * ptr)
    {
        std::stringstream ss;
        ss << std::showbase << std::hex << ptr;
        return ss.str();
    }

    inline bool ReadFileIntoString(std::string const &file_path,
                                   std::string &str)
    {
        std::ifstream ifs(file_path.c_str());
        if(!(ifs.good())) {
            // Not really clear if there's a robust
            // way to check if a file exists in C++
            ifs.close();
            return false;
        }

        // ref:
        // http://stackoverflow.com/questions/...
        // ...2602013/read-whole-ascii-file-into-c-stdstring

        str.clear();
        // @1 returns an iterator to the beginning of @ifs
        // @2 returns an EOS iterator
        str = std::string((std::istreambuf_iterator<char>(ifs) ),   // @1
                          (std::istreambuf_iterator<char>()    ));  // @2
        ifs.close();
        return true;
    }
}

#endif // KS_MISC_UTILS_H
