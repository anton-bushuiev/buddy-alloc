#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/*
 * An implementation of buddy memory allocator
 */

#define MAX_RANK 32

typedef struct block
{
  struct block * self;
  struct block * next;
  unsigned char rank;
  int free;
} SBlock;

static SBlock * g_List[MAX_RANK];
static SBlock * g_ExtraBlock;
static long int g_Start;
static int g_Size;
static unsigned char g_Rank;
static int g_Pending;

int log2lower( int arg )
{
  int retval = 0;

  while( arg >>= 1 )
    retval++;

  return retval;
}

int log2upper( int arg )
{
  int lower = log2lower( arg );
  if( 1 << lower < arg )
    lower++;
  return lower;
}

void addBlock( SBlock * block )
{
  block->next = g_List[block->rank];
  g_List[block->rank] = block;
}

void removeBlock( SBlock * block )
{
  if( ! g_List[block->rank] )
    return;

  if( g_List[block->rank] == block )
  {
    g_List[block->rank] = block->next;
    return;
  }

  SBlock * prev = g_List[block->rank];
  while( prev->next != block )
  {
    prev = prev->next;
    if( ! prev )
      return;
  }
  prev->next = block->next;

}

SBlock * split( SBlock * block )
{
  removeBlock( block );
  block->self = block;
  block->next = NULL;
  block->rank--;
  block->free = 1;
  // no add for return value

  SBlock * buddy;
  buddy = (SBlock *)( (long int)block + ( 1 << block->rank ) );
  buddy->self = buddy;
  buddy->next = NULL;
  buddy->rank = block->rank;
  buddy->free = 1;
  addBlock( buddy );

  return block;
}

SBlock * buddyOf( SBlock * block )
{
  long int pos = (long int) block - (long int) g_Start;

  return (SBlock * )( ( pos ^ ( 0x1 << (block->rank) ) ) + g_Start );
}

SBlock * join( SBlock * block )
{
  if( block->rank == g_Rank )
    return NULL;

  if( block == g_ExtraBlock )
    return NULL;

  SBlock * buddy = buddyOf( block );

  if( ! buddy->free || buddy->rank != block->rank )
    return NULL;

  removeBlock(block);
  removeBlock(buddy);

  if( buddy < block )
    block = buddy;

  block->self = block;
  block->next = NULL;
  block->rank++;
  block->free = 1;

  addBlock( block );

  return block;
}

void   HeapInit    ( void * memPool, int memSize )
{
  g_Start = (long int) memPool;

  g_Rank = log2lower( memSize );
  g_Size = 1 << g_Rank;

  for( int i = 0 ; i < MAX_RANK ; i++ )
    g_List[i] = NULL;

  SBlock * block = (SBlock *) memPool;
  block->self = block;
  block->next = NULL;
  block->rank = g_Rank;
  block->free = 1;
  addBlock( block );

  if( (unsigned) memSize > g_Size + sizeof(SBlock) )
  {
    g_ExtraBlock = (SBlock *) ( g_Start + g_Size );
    g_ExtraBlock->self = g_ExtraBlock;
    g_ExtraBlock->next = NULL;
    g_ExtraBlock->rank = log2lower( memSize - g_Size );
    g_ExtraBlock->free = 1;
    addBlock( g_ExtraBlock );
  }
  else
  {
    g_ExtraBlock = NULL;
  }

  g_Pending = 0;
}

void * HeapAlloc   ( int    size )
{
  int upper = log2upper( size + sizeof( SBlock ) );
  int r = upper;

  while( ! g_List[r] )
  {
    r++;
    if( r == MAX_RANK )
      return NULL;
  }

  SBlock * block = g_List[r];

  removeBlock( block );

  while( block->rank - 1 >= upper )
    block = split( block );

  block->free = 0;

  g_Pending++;

  return block + 1;
}

int   HeapFree    ( void * blk )
{
  SBlock * block = (SBlock *)( (long int) blk - sizeof( SBlock ) );

  if(block->self != block)
    return 0;

  block->free = 1;
  addBlock( block );

  while( block )
    block = join( block );

  g_Pending--;
  return 1;
}

void   HeapDone    ( int  * pendingBlk )
{
  * pendingBlk = g_Pending;
}

int main ( void )
{
  uint8_t       * p0, *p1, *p2, *p3, *p4;
  int             pendingBlk;
  static uint8_t  memPool[3 * 1048576];

  HeapInit ( memPool, 2097152 );

  assert ( ( p0 = (uint8_t*) HeapAlloc ( 512000 ) ) != NULL );
  memset ( p0, 0, 512000 );
  assert ( ( p1 = (uint8_t*) HeapAlloc ( 511000 ) ) != NULL );
  memset ( p1, 0, 511000 );
  assert ( ( p2 = (uint8_t*) HeapAlloc ( 26000 ) ) != NULL );
  memset ( p2, 0, 26000 );
  HeapDone ( &pendingBlk );
  assert ( pendingBlk == 3 );

  HeapInit ( memPool, 2097152 );
  assert ( ( p0 = (uint8_t*) HeapAlloc ( 1000000 ) ) != NULL );
  memset ( p0, 0, 1000000 );
  assert ( ( p1 = (uint8_t*) HeapAlloc ( 250000 ) ) != NULL );
  memset ( p1, 0, 250000 );
  assert ( ( p2 = (uint8_t*) HeapAlloc ( 250000 ) ) != NULL );
  memset ( p2, 0, 250000 );
  assert ( ( p3 = (uint8_t*) HeapAlloc ( 250000 ) ) != NULL );
  memset ( p3, 0, 250000 );
  assert ( ( p4 = (uint8_t*) HeapAlloc ( 50000 ) ) != NULL );
  memset ( p4, 0, 50000 );

  assert ( HeapFree ( p2 ) );
  assert ( HeapFree ( p4 ) );
  assert ( HeapFree ( p3 ) );
  assert ( HeapFree ( p1 ) );

  assert ( ( p1 = (uint8_t*) HeapAlloc ( 500000 ) ) != NULL );
  memset ( p1, 0, 500000 );
  assert ( HeapFree ( p0 ) );
  assert ( HeapFree ( p1 ) );
  HeapDone ( &pendingBlk );
  assert ( pendingBlk == 0 );

  HeapInit ( memPool, 2359296 );
  assert ( ( p0 = (uint8_t*) HeapAlloc ( 1000000 ) ) != NULL );
  memset ( p0, 0, 1000000 );
  assert ( ( p1 = (uint8_t*) HeapAlloc ( 500000 ) ) != NULL );
  memset ( p1, 0, 500000 );
  assert ( ( p2 = (uint8_t*) HeapAlloc ( 500000 ) ) != NULL );
  memset ( p2, 0, 500000 );
  assert ( ( p3 = (uint8_t*) HeapAlloc ( 500000 ) ) == NULL );
  assert ( HeapFree ( p2 ) );
  assert ( ( p2 = (uint8_t*) HeapAlloc ( 300000 ) ) != NULL );
  memset ( p2, 0, 300000 );
  assert ( HeapFree ( p0 ) );
  assert ( HeapFree ( p1 ) );
  HeapDone ( &pendingBlk );
  assert ( pendingBlk == 1 );

  HeapInit ( memPool, 2359296 );
  assert ( ( p0 = (uint8_t*) HeapAlloc ( 1000000 ) ) != NULL );
  memset ( p0, 0, 1000000 );
  assert ( ! HeapFree ( p0 + 1000 ) );
  HeapDone ( &pendingBlk );
  assert ( pendingBlk == 1 );

  return 0;
}
