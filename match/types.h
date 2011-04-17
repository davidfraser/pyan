#ifndef TYPES_H_
#define TYPES_H_

/* Order Id */
typedef unsigned long t_orderid;

/* Price
   0-65536 interpreted as divided by 100
   eg the range is 000.00-655.36 
   eg the price 123.45 = 12345
   eg the price 23.45 = 2345 
   eg the price 23.4 = 2340 */
typedef unsigned short t_price;

/* Order Size */
typedef unsigned long t_size;

/* Side 
   Ask=1, Bid=0 */
typedef int t_side;
int is_ask(t_side side) { return side; }

/* Limit Order */ 
typedef struct {
  char symbol[STRINGLEN];
  char trader[STRINGLEN];
  t_side side;
  t_price price;
  t_size size;
} t_order;

/* Execution Report 
   send one per opposite-sided order 
   completely filled */
typedef t_order t_execution;




#endif // TYPES_H_
