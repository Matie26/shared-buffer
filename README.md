# Solution of the producer-consumer problem in C using POSIX semaphores
### This code is an implementation of shared buffer that can work in both modes: LIFO / FIFO.
Shared memory problem, writing to full buffer and reading from empty buffer are solved by using 3 POSIX semaphores: _mutex_, _empty_ and _full_. 
In order to test this solution, 5 producers and 5 consumers are created and their task is to write/read given amount of data to/from the same buffer. When all of them finish, data read by consumers is written to new buffer (this time they act as producers) which is later checked for any duplicates. 

### Usage:
```./sharedBuffer [buffer_size] [buffer_mode (0:LIFO 1:FIFO)]```
