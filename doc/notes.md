* Since fault handlers run in tick context we should probably expose the critical section enter/exit via wrappers
  * Demo example should cover this: use critical section to protect whatever's written from the fault handler context
* If we add profiling, we'll need some concept of sub-tick timing
  * Port can implement some kind of get-timestamp, should be atomic or protected in some way
* For error handler callback we should have a cycle counter. Need to know what cycle the error occurred in