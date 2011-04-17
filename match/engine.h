#ifndef ENGINE_H_
#define ENGINE_H_

#include "limits.h"
#include "types.h"

// EXTERNAL

/* IN:
   OUT: */
void init();

/* IN:
   OUT: */
void destroy();

/* IN: order: limit order to add to book
   OUT: orderid assigned to order 
        start from 1 and increment with each call */
t_orderid limit(t_order order);

/* IN: orderid: id of order to cancel
   OUT:
   cancel request ignored if orderid not in book
*/
void cancel(t_orderid orderid);



// CALLBACKS

/* IN: execution: execution report 
   OUT: */
void execution(t_execution exec);


#endif // ENGINE_H_
