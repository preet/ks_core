### What is ks?
ks is a small cross platform c++ library that can be used to help create applications and libraries.

### What is ks_core?
The core module provides signals and slots with event loops, inspired by the Qt framework.

### License
ks is licensed under the Apache License, version 2.0. For more information see the LICENSE file.

### Dependencies
A modern c++11 compiler is required. Other dependencies include:

* [**asio**](http://www.think-async.com) (boost software license): used for event loops and timers

### Building
ks_core has a qmake pri file that can be added to a qmake project. The only dependency (asio) is header only and included in the module.

### Documentation
TODO. See the ks_test module for some examples
