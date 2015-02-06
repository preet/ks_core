![ks logo](http://preet.github.io/images/kslogo.png) 

### What is ks?
ks is a small generic helper lib that provides basic signals/slots and timers through event loops. It is greatly inspired by the Qt framework (but doesn't provide any of its advanced features like introspection)

### License
ks is licensed under the Apache License, version 2.0. For more information see the LICENSE file.

### Dependencies
ks is written in c++11 and requires a modern compiler. Tested with gcc 4.8.2. Other dependencies include:

* [**asio**](http://www.think-async.com) (boost software license): used for event loops and timers
* [**catch**](https://github.com/philsquared/Catch) (boost software license): for testing only, not required to use ks

### Building
ks is currently built using qmake but it should be straightforward to build using any build tools. The files can be directly added to any project and the dependencies (asio and catch) are header-only.

### Documentation
Currently TODO. See ks/test/KsTest.cpp for some examples.
