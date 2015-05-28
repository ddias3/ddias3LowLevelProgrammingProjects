#include "my_malloc.h"
#include <stdio.h>
#include <math.h>

/* You *MUST* use this macro when calling my_sbrk to allocate the 
 * appropriate size. Failure to do so may result in an incorrect
 * grading!
 */
#define SBRK_SIZE 2048

/* If you want to use debugging printouts, it is HIGHLY recommended
 * to use this macro or something similar. If you produce output from
 * your code then you will receive a 20 point deduction. You have been
 * warned.
 */
#ifdef DEBUG
#define DEBUG_PRINT(x) printf(x)
#else
#define DEBUG_PRINT(x)
#endif


/* make sure this always points to the beginning of your current
 * heap space! if it does not, then the grader will not be able
 * to run correctly and you will receive 0 credit. remember that
 * only the _first_ call to my_malloc() returns the beginning of
 * the heap. sequential calls will return a pointer to the newly
 * added space!
 * Technically this should be declared static because we do not
 * want any program outside of this file to be able to access it
 * however, DO NOT CHANGE the way this variable is declared or
 * it will break the autograder.
 */
void* heap;

/* our freelist structure - this is where the current freelist of
 * blocks will be maintained. failure to maintain the list inside
 * of this structure will result in no credit, as the grader will
 * expect it to be maintained here.
 * Technically this should be declared static for the same reasons
 * as above, but DO NOT CHANGE the way this structure is declared
 * or it will break the autograder.
 */
metadata_t* freelist[8];
/**** SIZES FOR THE FREE LIST ****
 * freelist[0] -> 16
 * freelist[1] -> 32
 * freelist[2] -> 64
 * freelist[3] -> 128
 * freelist[4] -> 256
 * freelist[5] -> 512
 * freelist[6] -> 1024
 * freelist[7] -> 2048
 */

/* Function that looks for an available block from the freelist
 *	always returns the block at the end of the list.
 * param availableBlockPointer = the pointer to the block that is found.
 * param whichFreelist = the list that the function searches through.
 */
int my_findAvailableBlock(metadata_t** availableBlockPointer, int whichFreelist)
{
	int foundBlock = 0;
	metadata_t* currentBlock = freelist[whichFreelist];
	
	if (NULL == currentBlock)
	{
		foundBlock = 0;
	}
	else
	{
		while (NULL != currentBlock->next)
			currentBlock = currentBlock->next;
		
		foundBlock = 1;
	}
	
	*availableBlockPointer = currentBlock;
	
	return foundBlock;
}

/* Function that looks for an available block from larger freelists
 *	starting at one larger than the desired block from the user.
 * param largerBlockPointer = the pointer to the block that is found.
 * param whichFreelist = the list that the block needs to be in.
 */
int my_findLargerBlock(metadata_t** largerBlockPointer, int whichFreelist)
{
	int counterFreelist = whichFreelist + 1;
	while (counterFreelist < 8)
	{
		if (my_findAvailableBlock(largerBlockPointer, counterFreelist))
		{
			break;
		}
		else
		{
			++counterFreelist;
		}
	}
	if (counterFreelist >= 8)
	{
		*largerBlockPointer = NULL;
		return -1;
	}
	
	return counterFreelist;
}

/* Function that removes a block from the freelist with all 4
 *	possible possibilities of next and prev pointers.
 * param block = the block that is removed from the freelist
 * param whichFreelist = the list that the block is being removed from.
 */
void my_removeBlockFromFreelist(metadata_t* block, int whichFreelist)
{
		//remove block from its current list
		//with all 4 possible combinations of next and previous
	if (NULL == block->next)
	{
		if (NULL == block->prev)
		{
			freelist[whichFreelist] = NULL;
		}
		else
		{
			block->prev->next = NULL;
		}
	}
	else
	{
		if (NULL == block->prev)
		{
			block->next->prev = NULL;
			freelist[whichFreelist] = block->next;
		}
		else
		{
			block->next->prev = block->prev;
			block->prev->next = block->next;
		}
	}
}

/* Function that splits a block until it is just big enough
 *	to fit the user's request. Takes the first half of the
 *	block to split on the next iteration.
 * param originalBlock = the first block that is what is split
 * param originalBlockFreelist = which freelist the originalBlock belonged to
 * param requiredSize = the size the user needs plus the metadata.
 * return metadata_t* the pointer to the block that fits the user's request
 */
metadata_t* my_splitBlock(metadata_t* originalBlock, int originalBlockFreelist, int requiredSize)
{
	int currentFreelist = originalBlockFreelist;
	
	metadata_t* half1Block = originalBlock;
	metadata_t* half2Block = originalBlock;
	
		// if current block's size divided by 2 is less than
		// the required size stop splitting the block
	while (((half1Block->size) >> 1) >= requiredSize)
	{
		my_removeBlockFromFreelist(half1Block, currentFreelist);
		
		//half1Block = half1Block;
		half2Block = (metadata_t*)(((half1Block->size) >> 1) + (size_t)half1Block);
		
		half1Block->size >>= 1; // size = size / 2;
		half1Block->in_use = 0;
		
		half2Block->size = half1Block->size;
		half2Block->in_use = half1Block->in_use;
		
		--currentFreelist;
		
			// readd half1Block to the freelist and half2Block
			// to the next of half1Block.
		metadata_t* currentBlock = freelist[currentFreelist];
		if (NULL == currentBlock)
		{
			freelist[currentFreelist] = half1Block;
			half1Block->next = half2Block;
			half1Block->prev = NULL;
			
			half2Block->prev = half1Block;
			half2Block->next = NULL;
		}
		else
		{
			while (NULL != currentBlock->next)
				currentBlock = currentBlock->next;
			
			currentBlock->next = half1Block;
			
			half1Block->prev = currentBlock;			
			half1Block->next = half2Block;
			
			half2Block->prev = half1Block;
			half2Block->next = NULL;
		}
		
			// if this is not commented, it splits
			// the second half of the block over and over
			// if commented, it splits
			// the first half of the block over and over
		//half1Block = half2Block;
	}
	
	return half2Block;
}

/* My version of malloc using the buddy system.
 * 	Takes blocks of size 2^n power [16, 2048]
 *	and splits them to be smallest possible the
 *	user can use.
 * param size = size of memory to be allocated in bytes.
 * return void* that the user uses.
 */
void* my_malloc(size_t size)
{
	metadata_t* userRequestMetadata = NULL;
	
		// first time setting up the heap with sbrk
	if (NULL == heap)
	{
		freelist[7] = heap = my_sbrk(SBRK_SIZE);
		
		if (heap == (void*)-1)
		{
			ERRNO = OUT_OF_MEMORY;
			return (void*)NULL;
		}
		
			// freelist[7] will always get it since it
			// carries blocks of size 2048
		freelist[7]->in_use = 0;
		freelist[7]->size = SBRK_SIZE;
		freelist[7]->next = NULL;
		freelist[7]->prev = NULL;
	}
	
		// (size + sizeof(metadata_t)) <= 16 * 2^userFreelist
		// log_2((size + sizeof(metadata_t))) / 16 <= userFreelist
	//int userFreelist = (int)log2f(((size + sizeof(metadata_t)) / 16));
	
		// This is the freelist that the user's request goes into.
		// it doesn't change after it is calculated here.
	int userFreelist;
	{
		int blockSize;
		for (userFreelist = 0, blockSize = 16; userFreelist < 8; ++userFreelist, blockSize *= 2)
			if (blockSize >= size + sizeof(metadata_t))
				break;
	}
	if (userFreelist >= 8)
	{
		ERRNO = SINGLE_REQUEST_TOO_LARGE;
		return (void*)NULL;
	}
	
		// Loop in case it requires more
		// memory from sbrk
	while (1)
	{
		metadata_t* currentBlock = NULL;
		
		// success on first try
		if (my_findAvailableBlock(&currentBlock, userFreelist))
		{
			userRequestMetadata = currentBlock;
			userRequestMetadata->in_use = 1;
			
			my_removeBlockFromFreelist(userRequestMetadata, userFreelist);
			break;
		}
		// Look for larger block
		else
		{
			int foundLargerBlockFreelist = my_findLargerBlock(&currentBlock, userFreelist);
			
			// Success
			if (foundLargerBlockFreelist >= 0)
			{
					// split and return left most of split blocks
				userRequestMetadata = my_splitBlock(currentBlock, foundLargerBlockFreelist, size + sizeof(metadata_t));
				userRequestMetadata->in_use = 1;
				
				my_removeBlockFromFreelist(userRequestMetadata, userFreelist);
				break;
			}
			// Call sbrk again for more space
			else
			{
					// Get the new 2048 byte large block and put it
					// at the end of the freelist[7] list, if it isn't empty.
				currentBlock = freelist[7];
				if (NULL == currentBlock)
				{
					freelist[7] = my_sbrk(SBRK_SIZE);
					
					if (freelist[7] == (void*)-1)
					{
						ERRNO = OUT_OF_MEMORY;
						return (void*)NULL;
					}
					
					freelist[7]->in_use = 0;
					freelist[7]->size = SBRK_SIZE;
					freelist[7]->next = NULL;
					freelist[7]->prev = NULL;
				}
				// freelist[7] isn't empty
				else
				{
					while (NULL != currentBlock->next)
						currentBlock = currentBlock->next;
										
					currentBlock->next = my_sbrk(SBRK_SIZE);
					
					if (currentBlock->next == (void*)-1)
					{
						ERRNO = OUT_OF_MEMORY;
						return (void*)NULL;
					}
					
					currentBlock->next->in_use = 0;
					currentBlock->next->size = SBRK_SIZE;
					currentBlock->next->next = NULL;
					currentBlock->next->prev = currentBlock;
				}
			}
		}
	}

	ERRNO = NO_ERROR;
	return (userRequestMetadata + 1);
}

/* My version of calloc that allocates a large chunk
 * 	big enough to fit num * size and sets 0 to all
 *	of the newly allocated bytes.
 * param num = the number of blocks to be allocated.
 * param size = size of each block of memory to be allocated.
 * return void* that the user uses.
 */
void* my_calloc(size_t num, size_t size)
{
	void* blockAllocated = my_malloc(num * size);
	
	if (NULL == blockAllocated)
	{
		//ERRNO already set by my_malloc
		return NULL;
	}
	
		// Set all bytes to zero
	int counter;
	for (counter = num * size - 1; counter >= 0; --counter)
	{
		((char*)blockAllocated)[counter] = 0;
	}
	
	ERRNO = NO_ERROR;
	return blockAllocated;
}

/* My version of free that sets the block's in_use variable to 0
 * 	and attempts to merge available blocks together.
 *	checks blocks 1 in front and 1 behind.
 * param ptr = the pointer the user gives to my_free to deallocate.
 */
void my_free(void* ptr)
{
	if (NULL == ptr)
		return;
	
	metadata_t* blockPointer = ((metadata_t*)ptr) - 1;
	
	if (blockPointer->in_use)
	{
		blockPointer->in_use = 0;
	}
	else
	{
		ERRNO = DOUBLE_FREE_DETECTED;
		return;
	}
	
		// This is the freelist that the user's request goes into.
		// it doesn't change after it is calculated here.
	int userFreelist;
	{
		int blockSize;
		for (userFreelist = 7, blockSize = 2048; userFreelist >= 0; --userFreelist, blockSize >>= 1)//blockSize /= 2)
			if (blockSize == blockPointer->size)
				break;
	}
	
		// loop to go through all of the freelists ending
		// before trying to combine blocks in freelist[7]
	int currentFreelist;
	for (currentFreelist = userFreelist; currentFreelist < 7; ++currentFreelist)
	{
			// check if sequential, then same size, then if paired correctly
			// this check is if the block ahead satisfies all 3 conditionals
		metadata_t* sequentialBlock = (metadata_t*)((size_t)blockPointer + blockPointer->size);
		
		if ((0 == sequentialBlock->in_use)
		&& (blockPointer->size == sequentialBlock->size)
			//subtracts heap from both pointers, then xors both numbers, bit wise negates, and ands the value with 1 bit shifted by n+1
		&& (~(((size_t)blockPointer - (size_t)heap) ^ ((size_t)sequentialBlock - (size_t)heap)) & (1 << (currentFreelist+4 + 1)))) //if share the same n+1 bit, where n = currentFreelist+4
		{
			my_removeBlockFromFreelist(sequentialBlock, currentFreelist);
			
			blockPointer->size <<= 1; //size *= 2;
		}
		else
		{
				// bound checking to make sure only memory greater
				// than the heap is accessed.
			if (((size_t)blockPointer - blockPointer->size) >= (size_t)heap)
			{
				sequentialBlock = (metadata_t*)((size_t)blockPointer - blockPointer->size);
				
				if ((0 == sequentialBlock->in_use)
				&& (blockPointer->size == sequentialBlock->size)
					// exact same as above
				&& (~(((size_t)blockPointer - (size_t)heap) ^ ((size_t)sequentialBlock - (size_t)heap)) & (1 << (currentFreelist+4 + 1))))
				{
					my_removeBlockFromFreelist(sequentialBlock, currentFreelist);
				
						// only difference if this succeeds instead of
						// the first check is that the sequentialBlock
						// is first and therefore more aligned, so use
						// it for the next iteration
					blockPointer = sequentialBlock;
					
					blockPointer->size <<=1; //size *= 2;
				}
				// if both block testings fail, break without joining blocks.
				else
				{
					break;
				}
			}
			// if bounds checking fails, break without joining blocks.
			else
			{
				break;
			}
		}
	}
	
		// readd blockPointer to the freelist
	metadata_t* freelistCurrentBlock = freelist[currentFreelist];
	if (NULL == freelistCurrentBlock)
	{
		freelist[currentFreelist] = blockPointer;
		blockPointer->next = NULL;
		blockPointer->prev = NULL;
	}
	else
	{
		while (NULL != freelistCurrentBlock->next)
			freelistCurrentBlock = freelistCurrentBlock->next;
		
		freelistCurrentBlock->next = blockPointer;
		blockPointer->next = NULL;
		blockPointer->prev = freelistCurrentBlock;
	}
	
	ERRNO = NO_ERROR;
	return;
}

/* My version of memmove that sets memory at one location to another
 * 	the order of bytes moved changes based on the value of the src and dest.
 *	the order they are copied means that it doesn't matter if they overlap.
 * param dest = the pointer where data will be copied into
 * param src = the pointer that has the data
 * param num_bytes = the number of bytes that will be copied
 * return returns the dest pointer
 */
void* my_memmove(void* dest, const void* src, size_t num_bytes)
{
		// Always start where they could overlap
		// Also, don't copy if src and dest are the same
	if ((size_t)src > (size_t)dest)
	{
		int counter;
		for (counter = 0; counter < num_bytes; ++counter)
		{
			((char*)dest)[counter] = ((char*)src)[counter];
		}
	}
	else if ((size_t)src < (size_t)dest)
	{
		int counter;
		for (counter = num_bytes-1; counter >= 0; --counter)
		{
			((char*)dest)[counter] = ((char*)src)[counter];
		}
	}
	return dest;
}
