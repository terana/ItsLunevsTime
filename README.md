# It's Lunev Time!

**Problems from MIPT DREC system programming course.**

*Here is the salvation from the nightmare of the bravest of the students of DREC MIPT: solutions to the problems by Lunev.*

## Contents
### 1. FIFO
Two programs, no parent-child relationship. *Producer* reads a file and passes it to *Consumer* via **FIFO**, then *Consumer* prints file contents to the screen.
### 2. Message queues.
The program produces N children (N is the command-line argument). After all of them are born, they print their numbers in order of appearance. Implementation with **message queue**.
### 3. Shared memory and semaphores.
Two programs, no parent-child relationship. *Producer* reads a file and passes it to *Consumer* via **shared memory**, then *Consumer* prints file contents to the screen.
### 4. Signals.
*The most perverted one.*

The program produces a child process, which opens a file and transmits its contents to the parent using only **signals**. Parent prints them to the screen.

*Yes, man! Data transmission with signals only!*
### 5. Polling.
The program produces N children. Each *Child* has a buffer of *blsize* bytes. For a *Child i* , *Parent* has a buffer of 3^(N - i) bytes.

*Child 0* reads the file and passes its contents to the *Parent*. *Parent* writes this data to the *buffer 0* (of the biggest size) and passes it to *Child 1*. *Child 1* gets the data and passes it back to *Parent*. *Parent* writes it to the *buffer 1* (of a smaller size). The schema repeats until the data is passed to *Child N-1*, which prints it to the screen.
