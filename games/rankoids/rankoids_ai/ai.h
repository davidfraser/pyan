#ifndef AI_H
#define AI_H


#include <stddef.h>


#define MAX_PLAYERS 7
#define DECK_SIZE 14

#define JOKER_VALUE ((DECK_SIZE)-1)
#define UNRANKED -1

typedef char DECK[DECK_SIZE];
typedef char *DECKP;

typedef int MOVE;

#define MAX_MOVES 100

#define MOVE_VALUE(move) ((move) >> 16)
#define MOVE_COUNT(move) ((move) & 0xFFFF)
#define MAKE_MOVE(value, count) (((value) << 16) | (count))

#define MOVE_INVALID MAKE_MOVE(DECK_SIZE, 0)
#define MOVE_PASS MAKE_MOVE(0, 0)
#define MOVE_JOKER MAKE_MOVE(JOKER_VALUE, 1)

//#define USE_TRANSPOSITION_TABLE
//#define USE_TRANSPOSITION_TABLE_SEARCH
#define TRANSPOSITION_TABLE_SIZE 1024*1024
#define TABLE_UNUSED -1000000

typedef struct PLAYER
{
    DECK hand;
    int rank;
} PLAYER;

typedef struct GAME
{
    int num_players;
    PLAYER players[MAX_PLAYERS];
    MOVE pile;
    int current_player;
    int pile_owner;
    int next_rank;
} GAME;

typedef struct PARAMETERS
{
    int depth;
    int playing_bonus;
    int total_score;
    int current_player_bonus;
    int hand_size_bonus;
    int different_card_bonus;
    int rank_bonus;
    int card_values[DECK_SIZE];
} PARAMETERS;

extern void clear_transposition_table(void);

extern PARAMETERS parameters;

extern int node_count;
extern int hit_count;


typedef struct STACK
{
    char *start;
    char *end;
    char *ptr;
} STACK;

extern STACK game_stack;

extern void init_stack(STACK *stack, size_t max_size);

extern void free_stack(STACK *stack);

extern void *stack_alloc(STACK *stack, size_t size);

extern void stack_pop(STACK *stack, void *ptr);


/**
 * Initialise a new game object.
 */
extern void initialise_game(GAME *game);


/**
 * Print a text representation of a game.
 */
extern void print_game(GAME *game);


/**
 * Print a deck of cards (e.g. a player's hand or the current pile).
 */
extern void print_deck(DECKP deck);


/**
 * Print a text representation of a move.
 */
extern void print_move(MOVE move);


/**
 * Clone a GAME object.
 */
extern GAME *clone_game(GAME *source);


/**
 * Generate all conceivable moves from a hand, including pass.
 */
extern int generate_all_moves(DECKP hand, MOVE *moves);


extern int is_valid_move(GAME *game, MOVE move);


/**
 * Generate all valid moves from a game.  These are moves from in the current
 * player's hand that are legal at this point in the game.
 */
extern int generate_valid_moves(GAME *game, MOVE *moves);


extern int get_hand_size(DECKP hand);


/**
 * Apply this move to the game.
 */
extern void apply_move(GAME *game, MOVE move);


extern int evaluate_hand(DECKP hand);


/**
 * Evaluate the game state for each player, storing the score in a vector.
 */
extern void evaluate_game_immediate(GAME *game, int *vector);


extern void evaluate_game(GAME *game, int *vector, int to_depth);


extern void evaluate_move(GAME *game, MOVE move, int *vector, int to_depth);


extern MOVE choose_move(GAME *game, int *best_vector, int to_depth);


#ifdef WIN32
#define EXPORT _declspec(dllexport)
#else
#define EXPORT
#endif

extern EXPORT void test(void);


#endif
