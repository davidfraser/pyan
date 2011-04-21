/** Edmund's price-time matching engine.
 *
 * See http://www.quantcup.org/ for background.
 *
 * Operations to optimise:
 *   - Finding matching orders (and removing them).
 *   - Adding an unmatched order.
 *   - Cancelling an order.
 *
 * First trick is to preallocate all data structures.  The maximum number of live orders is 65536.
 * Second trick is use an array of NODEs, one per price.  Price is limited to integers between 1 and 65536.
 * Third trick is to use a unified set of nodes for asks and bids.  At a given price, either bids or asks exist, but never both.
 * Fourth trick is to use a skip list for nodes.
 * Fifth trick is to use a hash table for orders in the queue, so we can find and cancel them.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "engine.h"

#define LIST_HEIGHT 4

#define MAP_SIZE MAX_LIVE_ORDERS

#ifdef NDEBUG
    #define fprintf(...)
#endif

typedef struct ORDER
{
    t_orderid id;
    t_order data;
    struct ORDER *next, *prev;
    struct ORDER *next_in_bin;
} ORDER;

typedef struct BIN
{
    ORDER *first, *last;
} BIN;

typedef struct NODE
{
    t_side side;
    t_price price;
    ORDER *first_order, *last_order;
    
    struct NODE *nexts[LIST_HEIGHT], *prevs[LIST_HEIGHT];
} NODE;


static NODE *nodes;
static NODE *bottom_node;
static NODE *top_node;

static NODE *best_bid;
static NODE *best_ask;

static ORDER *orders;

static BIN free_orders;
static BIN *id_map;

static t_orderid next_id = 0;

static char *order_string(t_order *data)
{
    static char buffer[100];
    snprintf(buffer, sizeof(buffer), "{ \"%s\", \"%s\", %d, %d, %ld }", data->symbol, data->trader, data->side, data->price, data->size);

    return buffer;
}

void init()
{
    int i;
    
    fprintf(stderr, "Initialising\n");
    
    nodes = malloc(sizeof(NODE) * (MAX_PRICE+2));
    for (i = MIN_PRICE; i <= MAX_PRICE; i++)
    {
        nodes[i].price = 0;
    }
    bottom_node = &nodes[0];
    top_node = &nodes[MAX_PRICE+1];
    bottom_node->price = 0;
    top_node->price = 0;
    for (i = 0; i < LIST_HEIGHT; i++)
    {
        bottom_node->nexts[i] = top_node;
        bottom_node->prevs[i] = NULL;
        top_node->nexts[i] = bottom_node;
        top_node->prevs[i] = NULL;
    }
    best_ask = top_node;
    best_bid = bottom_node;
    
    orders = malloc(sizeof(ORDER) * MAX_LIVE_ORDERS);
    for (i = 0; i < MAX_LIVE_ORDERS; i++)
    {
        orders[i].id = 0;
        orders[i].next_in_bin = &orders[i+1];
    }
    orders[MAX_LIVE_ORDERS-1].next_in_bin = NULL;
    free_orders.first = &orders[0];
    free_orders.last = &orders[MAX_LIVE_ORDERS-1];
    
    id_map = malloc(sizeof(BIN) * MAP_SIZE);
    memset(id_map, 0, sizeof(BIN) * MAP_SIZE);

    next_id = 1;
}

void destroy()
{
    fprintf(stderr, "Destroying\n");
    
    free(nodes);
    free(orders);
	free(id_map);
}

static void find_place(t_price price, NODE **vector)
{
    NODE *n = bottom_node;
    int i;
	int steps = 0;
    
    /*if (price >= best_ask->price)
        n = best_ask;*/
    
    for (i = LIST_HEIGHT - 1; i >= 0; i--)
    {
        while (n->nexts[i]->price <= price && n->nexts[i] != top_node)
        {
            n = n->nexts[i];
            steps++;
        }
		vector[i] = n;
    }

	if (steps >= 20)
	{
		printf("EEK, steps = %d\n", steps);
	}
}

static void add_to_list(NODE *node)
{
    int i;
    NODE *prevs[LIST_HEIGHT];
    
    find_place(node->price, prevs);

	for (i = 0; i < LIST_HEIGHT; i++)
	{
		node->prevs[i] = prevs[i];
		node->nexts[i] = prevs[i]->nexts[i];
		prevs[i]->nexts[i]->prevs[i] = node;
		prevs[i]->nexts[i] = node;

		if ((rand() % 4) != 0)
			break;
	}
    
    for (i = i+1; i < LIST_HEIGHT; i++)
    {
        node->nexts[i] = NULL;
        node->prevs[i] = NULL;
    }

    if (is_ask(node->side) && (node->price < best_ask->price || best_ask == top_node))
    {
        int i;
        /*for (i = LIST_HEIGHT-1; i >= 1; i++)
        {
            if (node->nexts[i] == NULL && best_ask != top_node)
            {
                node->nexts[i] = best_ask;
                best_ask->prevs[i]->nexts[i] = node;
            }
            if (node->prevs[i] == NULL)
            {
                node->prevs[i] = best_ask->prevs[i];
                best_ask->prevs[i] = node;
            }
        }*/
        best_ask = node;
    }
    else if (!is_ask(node->side) && (node->price > best_bid->price || best_bid == bottom_node))
        best_bid = node;
}

/**
 * Remove a node from the skip list.
 * N.B. The node's nexts[0] and prevs[0] pointers remain intact after this!
 */
static void remove_from_list(NODE *node)
{
    int i;
	assert(node->nexts[0] != NULL);
	assert(node->prevs[0] != NULL);
    node->price = 0;
    for (i = 0; i < LIST_HEIGHT; i++)
    {
        if (node->nexts[i])
            node->nexts[i]->prevs[i] = node->prevs[i];
        if (node->prevs[i])
            node->prevs[i]->nexts[i] = node->nexts[i];
    }

    if (node == best_bid)
    {
        best_bid = best_bid->prevs[0];
		assert(best_bid != NULL);
        assert((best_bid != bottom_node) != (best_bid->price == 0));
    }
    if (node == best_ask)
    {
        best_ask = best_ask->nexts[0];
		assert(best_ask != NULL);
        assert((best_ask != top_node) != (best_ask->price == 0));
    }
}

static ORDER *allocate_order(t_orderid id)
{
    ORDER *order;
    int h;
    
    assert(free_orders.first);
    
    order = free_orders.first;
    free_orders.first = order->next_in_bin;
    if (free_orders.first == NULL)
        free_orders.last = NULL;
    order->id = next_id;
    
    h = order->id % MAP_SIZE;
    order->next_in_bin = NULL;
    if (id_map[h].first == NULL)
    {
        id_map[h].first = order;
    }
    else
    {
        id_map[h].last->next_in_bin = order;
    }
    id_map[h].last = order;
    
    return order;
}

static ORDER *free_order(t_orderid id)
{
    int h = id % MAP_SIZE;
    ORDER *order = id_map[h].first;
    ORDER *prev = NULL;
    int i = 0;
    while (order != NULL)
    {
        if (order->id == id)
            break;
        prev = order;
        order = order->next_in_bin;
        i++;
    }
    if (i > 10)
        printf("i = %d!\n", i);
    
    if (order == NULL)
        return NULL;
    
    if (prev == NULL)
    {
        id_map[h].first = order->next_in_bin;
        if (id_map[h].first == NULL)
            id_map[h].last = NULL;
    }
    else
    {
        prev->next_in_bin = order->next_in_bin;
    }
    order->next_in_bin = NULL;
    free_orders.last->next_in_bin = order;
    free_orders.last = order;
    
    return order;
}

/**
 * 1. Clear order id.
 * 2. Remove the order from the list for that price.
 * 3. If list is now empty, remove that price.
 * 4. Add order to free orders stack.
 */
static void remove_order(ORDER *order)
{
    NODE *node;
    
    order->id = 0;
    
    node = &nodes[order->data.price];
    assert(node->price == order->data.price);
    
    if (order->next != NULL)
        order->next->prev = order->prev;
    else
        node->last_order = order->prev;
        
    if (order->prev != NULL)
        order->prev->next = order->next;
    else
        node->first_order = order->next;
    
    if (node->first_order == NULL)
    {
        fprintf(stderr, "Last order removed, removing node from list\n");
        remove_from_list(node);
    }
}

/* Call the execution report callback to
   notify both parties to the trade 
   o1 and o2 are assumed to be the same price
   on opposite sides */
void send_exec(t_order * o1, t_order * o2) {
  t_execution exec = *(t_execution *)o1;
  int i;
    fprintf(stderr, "Executing order %s\n", order_string(o1));
    fprintf(stderr, "        against %s\n", order_string(o2));
  exec.size = o1->size > o2->size ? o2->size : o1->size;
  execution(exec);
  // rename trader field on exec to old's name
  for(i = 0; i < STRINGLEN; i++) {
    exec.trader[i] = o2->trader[i]; 
  } 
  exec.side = !exec.side;
  execution(exec);  
}



/**
 * Attempt to consume a node.
 * If the node is completely consumed, remove it from the list.
 * If completely consumed, and data still has size, return 1.
 */
static int consume_node(NODE *node, t_order *data)
{
    ORDER *order;
    while (data->size > 0 && node->first_order)
    {
        order = node->first_order;
        send_exec(data, &order->data);
        
        if (data->size < order->data.size)
        {
            order->data.size -= data->size;
            data->size = 0;
            fprintf(stderr, "New order is exhausted\n");
            break;
        }
        
        data->size -= order->data.size;
        fprintf(stderr, "Old order consumed\n");
        free_order(order->id);
        remove_order(order);
    }
    return data->size > 0;
}

static int cross(t_order *data)
{
    if (is_ask(data->side))
    {
        while (best_bid != bottom_node && best_bid->price >= data->price)
        {
            if (!consume_node(best_bid, data))
                break;
        }
    }
    else
    {
        while (best_ask != top_node && best_ask->price <= data->price)
        {
            if (!consume_node(best_ask, data))
                break;
        }
    }
    
    return data->size == 0;
}

static void queue(t_order *data)
{
    ORDER *order;    
    NODE *node = &nodes[data->price];
    
    fprintf(stderr, "Queueing %s\n", order_string(data));
    if (node->price == 0)
    {
        node->price = data->price;
        node->side = data->side;
        node->first_order = NULL;
        node->last_order = NULL;
        
        add_to_list(node);
    }
    else
    {
        assert(node->side == data->side);
        assert(node->price == data->price);
    }
    
    order = allocate_order(next_id);
    order->data = *data;

    order->prev = node->last_order;
    order->next = NULL;
    if (node->last_order)
        node->last_order->next = order;
    node->last_order = order;
    if (node->first_order == NULL)
        node->first_order = order;
}

t_orderid limit(t_order data)
{
    fprintf(stderr, "Placing order: %s\n", order_string(&data));
    if (!cross(&data))
        queue(&data); 
    fprintf(stderr, "Order id is %ld\n", next_id);
    return next_id++;
}

void cancel(t_orderid id)
{
	ORDER *order;
    fprintf(stderr, "Cancelling order: %ld\n", id);
    order = free_order(id);
    if (order)
    {
        fprintf(stderr, "Cancelling %s\n", order_string(&order->data));
        remove_order(order);
    }
}

#ifdef NDEBUG
    #undef fprintf
#endif
