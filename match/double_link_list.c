/*
#ifdef TESTING
#define MYDATA int
#define MYDATAINIT 0
#endif
*/

#include <stdlib.h>

struct node_struct {
  MYDATA data;
  struct node_struct * prev;
  struct node_struct * next;
};
typedef struct node_struct node;

typedef struct ll {
  node * head;
  node * tail;
  size_t size;
} list;

/*
  Create a new node with the given data and links
  Returns a pointer to the new node, or NULL on error
*/
node * new_node(MYDATA data, node * prev, node * next) {
  node * rv = malloc(sizeof(node));
  rv->data = data;
  rv->prev = prev;
  rv->next = next;
  return rv;
}

/*
  Create a new node with the given data and links
  Returns a pointer to the new node, or NULL on error
*/
node * new_node_empty(node * prev, node * next) {
  node * rv = malloc(sizeof(node));
  //  memset(&rv->data, 0, sizeof(rv->data));
  rv->prev = prev;
  rv->next = next;
  return rv;
}

/*
  Create a new list with an optional dummy head and tail
  Returns a pointer to the new list, or NULL on error
*/
list * new_list() {
  list * rv = malloc(sizeof(node));
  node * tail = new_node_empty(NULL, NULL);
  rv->head = new_node_empty(NULL, tail);
  
  /* Finish linking the tail to the head */
  rv->tail = tail;
  rv->tail->prev = rv->head;
  rv->size = 0;
  return rv;
}
 
/*
  Destroy all nodes in a given list
  Optionally destroy all data in each node
*/
void delete_list(list * list) {
  while(list->head != NULL) {
    node * head = list->head;
    list->head = list->head->next;
    free(head);
  }
}

node * insert_before(list * list, node * pos, MYDATA data);
node * insert_after(list * list, node * pos, MYDATA data);

/*
  Insert a new node after the given node
  Returns a pointer to the new node or NULL on failure
*/
node * insert_after(list * list, node * pos, MYDATA data) {
  node * rv = NULL;
  
  if(list != NULL && pos != NULL) {
    if(pos != list->tail) {
      /* Create a new node and set the next link */
      rv = new_node(data, pos, pos->next);
      
      if(rv != NULL) {
	if(pos->next != NULL) {
	  pos->next->prev = rv;
	}
	pos->next = rv;
	++list->size;
      }
    }
    else {
      rv = insert_before(list, pos, data);
    }
  } 
  return rv;
}
 
/*
  Insert a new node before the given node
  Returns a pointer to the new node or NULL on failure
*/
node * insert_before(list * list, node * pos, MYDATA data) {
  node * rv = NULL;
 
  if(list != NULL && pos != NULL){
    if(pos != list->head){
      /* Create a new node and set the next link */
      rv = new_node(data, pos->prev, pos);
      
      if(rv != NULL){
	if(pos->prev != NULL) {
	  pos->prev->next = rv;
	}	
	pos->prev = rv;
	++list->size;
      }
    }
    else {
      rv = insert_after(list, pos, data);
    }
  }
  return rv;
}

/*
  Remove the selected node
  Returns the removed node or NULL on failure
*/
node * remove_node(list * list, node * pos) {
  node * rv = NULL;
  
  if(list != NULL && pos != NULL) {
    /* Shift off of the dummies */
    if(pos == list->head) {
      pos = pos->next;
    }   
    if(pos == list->tail) {
      pos = pos->prev;
    }
    if(pos != list->head) {
      /* Remove a non-dummy node */
      rv = pos;
      
      /* Reset the list links if necessary */
      if(rv->prev != NULL)
        rv->prev->next = rv->next;
      
      if(rv->next != NULL)
        rv->next->prev = rv->prev;
      
      /* Clean up the old node */
      free(rv);
      //      rv->prev = rv->next = NULL;
      --list->size;
    }
  }
  
  return rv;
}

/*
#ifdef TESTING
// Test the doubly linked list implementation
#include <stdio.h> 
int main() {
  list * l = new_list();
  insert_after(l, l->head, 1);
  insert_after(l, l->head->next, 2);
  insert_after(l, l->head->next, 3);
  insert_before(l, l->head->next, 4);
  remove_node(l, l->head);
  printf("%i\n", l->head->data);
  printf("%i\n", l->head->next->data);
  printf("%i\n", l->head->next->next->data);
  printf("%i\n", l->head->next->next->next->data);
  printf("%i\n", l->head->next->next->next->next->data);

  delete_list(l);
  printf("%i\n", l->head);
  printf("%i\n", l->tail->data);
  // should be
  // 0
  // 1
  // 3
  // 2
  // 0
  // 0
  // /garbage value/
  //

}
#endif // TESTING
*/
