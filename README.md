5G-EmPOWER: Mobile Networks Operating System
=========================================

# What is 5G-EmPOWER?

5G-EmPOWER is a mobile network operating system designed for heterogeneous wireless/mobile networks.

# Top-Level Features

* Supports both LTE and WiFi radio access networks
* Northbound abstractions for a global network view, network graph, and application intents.
* REST API and native (Python) API for accessing the Northbound abstractions
* Support for Click-based Lightweight Virtual Networks Functions
* Declarative VNF chaning on precise portion of the flowspace
* Flexible southbound interface supporting WiFi APs and LTE eNBs

Checkout out our [website](http://5g-empower.io/) and our [wiki](https://github.com/5g-empower/5g-empower.github.io/wiki)

This repository includes the 5G-EmPOWER eNodeB agent library.

## Agent

The Agent comes in the shape of a (shared) library which must be included in the eNodeB stack. The Agent must be interfaced with the eNB through the implementation of a set of functionalities that will be passed to it (the Agent) during its bootstrap (we call this part the "wrapper").

There are no restrictions on the wrapper implementation, and you are free to choose the strategy which better fit the implementation of your Base Station.

Some example on how the Agent can be integrated can be found in the **srsLTE** repository (example of integration in a real eNB software).

# How To Use

## Pre-requisites

In order to successfully build the 5G-EmPOWER eNB Agent you need to install the linux build tools

`sudo apt-get install build-essential libpthread-stubs0-dev`

You will also need to have the OpenEmpower protocols correctely compiled and installed in or to move proceed.

## Build from source

The standard build assumes that you want to install the software and the necessary headers in the default directories `/usr/include/emage` for headers, and `/usr/lib` for the shared objects.

You can the defaults by modifying the `INCLDIR` and `INSTDIR` variables present in the Makefile .

To build the software run the following command:

`make`

If you desire to inspect/debug the Agent internals, you can compile the library in "debug mode":

`make debug`

Finally, if you desire even more verbosity while debugging (see the RAW messages sent), you can run:

`make verbose`

## Install

After having built the software, to install it run the following command:

`sudo make install`

This will copy the shared library within the `/usr/lib` folder. By doing this, you will be able to include the Agent in your projects just by compiling it with the additional flag `-lemagent`.

The second action taken by the install command is copying the headers of the Agent inside `/usr/include/emage` folder. This will allows your projects to reach the Agent definitions from within your code by including the Agent header as it follows: `#include <emage/emage.h>`.

## Uninstalling

You can uninstall the software run the following command:

`sudo make uninstall`

## License

Code is released under the Apache License, Version 2.0.
