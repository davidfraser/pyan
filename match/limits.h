/* Maximum price of a limit order 
   655.36 dollars 
   max size of unsigned short */
#define MAX_PRICE 65536

/* Minimum price of a limit order 
   0.01 dollars */
#define MIN_PRICE 00001

/* Maximum number of uncrossed orders that
   may be sitting on the book at a time.
   Gives the implementor a finite bound */
#define MAX_LIVE_ORDERS 65536

/* Maximum number of characters in both
   symbol and trader fields in order */
#define STRINGLEN 5
