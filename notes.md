# Object Allocator (Notes taken from class)
Read *everything* from the website. This is important to ensure that the details are not wrong and the not tested cases will work.

## Signing the allocated memory
The signing of memory is used to indicate who is managing that block of memory at that time in the program. This is only done during debug as that is the only time where the memory will be read.

- AA is memory which has not been allocated for the user yet
- BB is memory given to the user
- CC is memory the user released after using it
- DD is memory which is being used to surround the data blocks in 
