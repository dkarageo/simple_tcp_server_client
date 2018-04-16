# **Simple TCP Server/Client**

## A simple implementation of a TCP server and client developed for educational reasons.


Developed by *Dimitrios Karageorgiou*,\
during the course *Embedded and Realtime Systems*,\
*Aristotle University Of Thessaloniki, Greece,*\
*2017-2018.*


Actually, this repository contains two server implementations. Both are able to handle multiple clients simultaneously, though they do it using different tools. The one spawns a new thread for each client while the other spawns a new process.

The implementations are robust on termination dealing with many data loss possibilities.

The implementation with processes is targeting and is expected to operate correctly on systems having a valid definition for *_POSIX_REALTIME_SIGNALS*, like modern linux kernels.
