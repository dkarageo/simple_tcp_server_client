# **Simple TCP Server/Client**

## A simple implementation of a TCP server and client developed for educational reasons.


Developed by *Dimitrios Karageorgiou*,\
during the course *Embedded and Realtime Systems*,\
*Aristotle University Of Thessaloniki, Greece,*\
*2017-2018.*


Actually, this repository contains two server implementations. Both are able to handle multiple clients simultaneously, though they do it using different tools. The one spawns a new thread for each client while the other spawns a new process.

Currently, the implementation using threads is more robust since it handles more edge cases. On implementation with processes many things have not been implemented, since it would need an extreme amount of work compared to work needed for the one with threads, for actually being more slow and resource consuming.

 
