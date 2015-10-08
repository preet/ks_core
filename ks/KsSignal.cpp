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

#include <ks/KsSignal.hpp>

namespace ks 
{
	namespace signal_detail
	{
        // Start at one so that an Id of 0
		// can be considered invalid / unset
        Id g_cid_counter(1);
        std::mutex g_cid_mutex;
		
		Id genId()
		{
            std::lock_guard<std::mutex> lock(g_cid_mutex);
            Id id = g_cid_counter;
            g_cid_counter++;
            return id;
		}
		
	} // signal_detail

} // ks
