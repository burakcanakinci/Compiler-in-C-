// RUN: AArch64

// FUNC-DECL: int test()
// TEST-CASE: test() -> 1

/**
 * Kyler Smith, 2017
 * Stack data structure implementation.
 */

////////////////////////////////////////////////////////////////////////////////
// INCLUDES
#include <stdio.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// MACROS: CONSTANTS
#define NULL 0

////////////////////////////////////////////////////////////////////////////////
// DATA STRUCTURES
/**
 * creating a stucture with 'data'(type:int), two pointers 'next','pre' (type: struct node) .
 */
struct node
{
    int data;
    struct node *next;
    struct node *pre;
} * head, *tmp;

////////////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
int count = 0;

////////////////////////////////////////////////////////////////////////////////
// FUNCTION PROTOTYPES
void create();
void push(int x);
int pop();
int peek();
int size();
int isEmpty();

////////////////////////////////////////////////////////////////////////////////
// MAIN ENTRY POINT

int test()
{
    int x, y, z;

    create();
    push(4);
    x = pop();
    // 4. Count: 0. Empty: 1.
    if (x != 4 || size() != 0 || !isEmpty()) {
      printf("%d.\t\tCount: %d.\tEmpty: %d.\n", x, size(), isEmpty());
      return 0;
    }

    push(1);
    push(2);
    push(3);
    x = pop();
    y = pop();
    // 3, 2. Count: 1. Empty: 0;
    if (x != 3 || y != 2 || size() != 1 || isEmpty()) {
      printf("%d, %d.\t\tCount: %d.\tEmpty: %d.\n", x, y, size(), isEmpty());
      return 0;
    }
    
    pop();  // Empty the stack.

    push(5);
    push(6);
    x = peek();
    push(7);
    y = pop();
    push(8);
    z = pop();
    // 1, 6, 7, 8. Count: 2. Empty: 0.
    if (x != 6 || y != 7 || z != 8 || size() != 2 || isEmpty()) {
      printf("%d, %d, %d.\tCount: %d.\tEmpty: %d.\n", x, y, z, size(), isEmpty());
      return 0;
    }

    return 1;
}

/**
 * Initialize the stack to NULL.
 */
void create() { head = NULL; }

/**
 * Push data onto the stack.
 */
void push(int x)
{
    if (head == NULL)
    {
        head = (struct node *)malloc(1 * sizeof(struct node));
        head->next = NULL;
        head->pre = NULL;
        head->data = x;
    }
    else
    {
        tmp = (struct node *)malloc(1 * sizeof(struct node));
        tmp->data = x;
        tmp->next = NULL;
        tmp->pre = head;
        head->next = tmp;
        head = tmp;
    }
    ++count;
}

/**
 * Pop data from the stack
 */
int pop()
{
    int returnData;
    if (head == NULL)
    {
        printf("ERROR: Pop from empty stack.\n");
        exit(1);
    }
    else
    {
        returnData = head->data;

        if (head->pre == NULL)
        {
            free(head);
            head = NULL;
        }
        else
        {
            head = head->pre;
            free(head->next);
        }
    }
    --count;
    return returnData;
}

/**
 * Returns the next value to be popped.
 */
int peek()
{
    if (head != NULL)
        return head->data;
    else
    {
        printf("ERROR: Peeking from empty stack.");
        exit(1);
    }
}

/**
 * Returns the size of the stack.
 */
int size() { return count; }

/**
 * Returns 1 if stack is empty, returns 0 if not empty.
 */
int isEmpty()
{
    if (count == 0)
        return 1;
    return 0;
}
