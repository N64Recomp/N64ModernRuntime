# N64 Modern Runtime

> Note
This repo is a WIP as files are moved out of Zelda64Recomp and genericized. It cannot be used directly in its current state.

This repo contains two libraries: Ultramodern and Librecomp.

## Ultramodern

Ultramodern is a reimplementation of much of the core functionality of libultra. It can be used with either statically recompiled projects that use N64Recomp or direct source ports. It implements the following libultra functionality:

* Threads
* Controllers
* Audio
* Message Queues
* Timers
* RSP Task Handling
* VI timing

Platform-specific I/O is handled via callbacks that are provided by the project using ultramodern. This includes reading from controllers and playing back audio samples.

ultramodern expects the user to provide and regiter a graphics renderer. The recommended one is [RT64](https://github.com/rt64/rt64).

## Librecomp

Librecomp is a library meant to be used to bridge the gap between code generated by N64Recomp and ultramodern. It provides wrappers to allow recompiled code to call ultramodern. Librecomp also provides some of the remaining libultra functionality that ultramodern doesn't provide, which includes:

* Overlay handling
* PI DMA (ROM reads)
* EEPROM, SRAM and Flashram saving (these may be partially moved to ultramodern in the future)