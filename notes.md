# Object Allocator (Notes taken from class)
Read *everything* from the website. This is important to ensure that the details are not wrong and the not tested cases will work.

## Signing the allocated memory
The signing of memory is used to indicate who is managing that block of memory at that time in the program. This is only done during debug as that is the only time where the memory will be read.

- AA is memory which has not been allocated for the user yet.
- BB is memory given to the user.
- CC is memory the user released after using it.
- DD is memory which is being used to surround each object. This is used to do bound checking when writing to the data. Pad bytes exist in both sides of each block of data. The recommended padding is 64.
- EE is the memory being used to ensure alignment is met for each object. (To be covered later on)

## Header blocks
A header block can hold data to keep track of information regarding the block. This is important to make sure there are no undesired overwrites or double deallocations.

The header block can be seen in the struct `MemBlockInfo` as it stores not only the flag but also a bit more information.
