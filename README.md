# waitfor   -   command line tool for waiting until a host is reachable


For example:
---

* waitfor [host] [port] (waits until host is reachable at port)
* waitfor -t30 [host] [port] (waits until host is reachable at port, stops after 30 seconds if still not reachable)
* waitfor [host] (pings until host is reachable)
* waitfor -t30 [host] (pings until host is reachable, stops after 30 seconds if still not reachable)

Exit codes:
---

* 0 = host is reachable
* 1 = host is not reachable
* 2 = waitfor received SIGTERM/SIGINT
* 3 = other error

Please take a look in the Makefile for compilation instructions.

