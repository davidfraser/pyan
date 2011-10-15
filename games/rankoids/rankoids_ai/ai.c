#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "ai.h"


STACK game_stack;

void init_stack(STACK *stack, size_t max_size)
{
    stack->start = malloc(max_size);
    stack->end = stack->start + max_size;
    stack->ptr = stack->start;
}

void free_stack(STACK *stack)
{
    free(stack->start);
}

void *stack_alloc(STACK *stack, size_t size)
{
    void *ptr;
    if (stack->ptr + size > stack->end)
        return NULL;
    
    ptr = stack->ptr;
    stack->ptr += size;
    return ptr;
}

void stack_pop(STACK *stack, void *ptr)
{
    stack->ptr = ptr;
}


int transposition_table[TRANSPOSITION_TABLE_SIZE][MAX_PLAYERS];


void clear_transposition_table(void)
{
    int i;

    for (i = 0; i < TRANSPOSITION_TABLE_SIZE; i++)
        transposition_table[i][0] = TABLE_UNUSED;
}


PARAMETERS parameters = {
    10,
    300,
    1000,
    100,
    -10,
    -10,
    1000,
    { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 20 }
};


int node_count;
int hit_count;


char *card_names[] = {
    "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A", "2", "Joker"
};


void initialise_game(GAME *game)
{
    int i;
    memset(game, 0, sizeof(GAME));

    for (i = 0; i < MAX_PLAYERS; i++)
        game->players[i].rank = UNRANKED;
}


void print_game(GAME *game)
{
    int i;

    printf("GAME(%x), %d players, current %d, owner %d, pile ", game, game->num_players, game->current_player, game->pile_owner);
    print_move(game->pile);
    printf("\n");

    for (i = 0; i < game->num_players; i++)
    {
        printf("    rank %d, hand ", game->players[i].rank);
        print_deck(game->players[i].hand);
        printf("\n");
    }
}


void print_deck(DECKP deck)
{
    int i, j;
    int printed_one = 0;
    for (i = 0; i < DECK_SIZE; i++)
        for (j = 0; j < deck[i]; j++)
        {
            if (printed_one)
                printf(" ");
            printf("%s", card_names[i]);
            printed_one = 1;
        }
}


void print_move(MOVE move)
{
    int value = MOVE_VALUE(move);
    int count = MOVE_COUNT(move);
    int i;

    if (move == MOVE_INVALID)
    {
        printf("invalid");
        return;
    }

    printf("[");
    for (i = 0; i < count; i++)
    {
        if (i > 0)
            printf(" ");
        printf("%s", card_names[value]);
    }
    printf("]");
}


GAME *clone_game(GAME *source)
{
    GAME *target = stack_alloc(&game_stack, sizeof(GAME));
    memcpy(target, source, sizeof(GAME));
    
    return target;
}


int generate_all_moves(DECKP hand, MOVE *moves)
{
    int i, j;
    int num = 0;
    
    moves[num++] = MOVE_PASS;
    
    for (i = 0; i < DECK_SIZE; i++)
    {
        if (i == JOKER_VALUE && hand[i] > 0)
            moves[num++] = MOVE_JOKER;
        else
            for (j = 1; j <= hand[i]; j++)
                moves[num++] = MAKE_MOVE(i,j );
    }
    
    return num;
}


int is_valid_move(GAME *game, MOVE move)
{
    int move_value;
    int move_count;
    int pile_value;
    int pile_count;

    /* Pass is valid if the pile belongs to someone else, or the player is out. */
    if (move == MOVE_PASS)
    {
        if (get_hand_size(game->players[game->current_player].hand) == 0)
            return 1;
        
        return (game->current_player != game->pile_owner);
    }
    
    /* Joker is always valid. */
    if (move == MOVE_JOKER)
    {
        return 1;
    }

    /* If the pile is empty or belongs to us, the move is valid. */
    pile_count = MOVE_COUNT(game->pile);

    if (pile_count == 0 || game->pile_owner == game->current_player)
    {
        return 1;
    }

    /* Otherwise, if the move is the same size of pile but higher value,
       then it's is valid. */
    pile_value = MOVE_VALUE(game->pile);
    move_value = MOVE_VALUE(move);
    move_count = MOVE_COUNT(move);

    if (move_count == pile_count && move_value > pile_value)
        return 1;

    return 0;
}


int generate_valid_moves(GAME *game, MOVE *moves)
{
    DECKP hand = game->players[game->current_player].hand;
    int i;
    int num = 0;
    MOVE all_moves[MAX_MOVES];
    int num_moves;

    num_moves = generate_all_moves(hand, all_moves);
    
    /* Special case: put highest move first, just in case it helps pruning. */
    if (num_moves >= 1 && is_valid_move(game, all_moves[num_moves-1]))
       moves[num++] = all_moves[num_moves-1];
    
    for (i = 0; i < num_moves-1; i++)
        if (is_valid_move(game, all_moves[i]))
            moves[num++] = all_moves[i];

    return num;
}


int get_hand_size(DECKP hand)
{
    int i;
    int size = 0;

    for (i = 0; i < DECK_SIZE; i++)
        size += hand[i];

    return size;
}


void apply_move(GAME *game, MOVE move)
{
    PLAYER *player = &game->players[game->current_player];
    int next_player;
    
    if (move != MOVE_PASS)
    {
        int move_value = MOVE_VALUE(move);
        int move_count = MOVE_COUNT(move);

        DECKP hand = player->hand;

        hand[move_value] -= move_count;

        game->pile = move;
        game->pile_owner = game->current_player;
        
        /* Is this player out? */
        if (get_hand_size(player->hand) == 0)
        {
            player->rank = game->next_rank;
            game->next_rank++;
        }
    }
    
    /* Move to next player. */
    next_player = game->current_player;
    do
    {
        next_player = (next_player+1) % game->num_players;
        if (next_player == game->current_player)
        {
            /* Game is over. */
            game->players[next_player].rank = game->next_rank;
            return;
        }
    }
    while (game->players[next_player].rank != UNRANKED);
    
    game->current_player = next_player;
}


int game_is_over(GAME *game)
{
    return game->next_rank >= game->num_players-1;
}


int evaluate_hand(DECKP hand)
{
    int first_pos = 0, last_pos = DECK_SIZE;
    int median_pos;
    int median_value;
    int score = parameters.playing_bonus;
    int i;
    
    while (first_pos < DECK_SIZE && hand[first_pos] == 0)
        first_pos++;

    while (last_pos > first_pos && hand[last_pos] == 0)
        last_pos--;
    
    median_pos = (first_pos + last_pos)/2;
    median_value = parameters.card_values[median_pos];
    
    for (i = 0; i < DECK_SIZE; i++)
    {
        if (hand[i] > 0)
        {
            int card_score = parameters.card_values[i] - median_value;
            score += hand[i] * (card_score + parameters.hand_size_bonus);
            score += parameters.different_card_bonus;
        }
    }
    
    return score;
}


unsigned int hash_game(GAME *game)
{
    unsigned int hash = 47;
    int *words = (int *) game;
    int num_words = sizeof(GAME) / sizeof(int);
    int i;
    
    for (i = 0; i < num_words; i++)
    {
        hash += 83 * (words[i] + i);
    }

    return hash;
}


void evaluate_game_immediate(GAME *game, int *vector)
{
    int i;
    unsigned int hash;

#ifdef USE_TRANSPOSITION_TABLE
    hash = hash_game(game) % TRANSPOSITION_TABLE_SIZE;
    if (transposition_table[hash][0] != TABLE_UNUSED)
    {
        memcpy(vector, transposition_table[hash], sizeof(int)*MAX_PLAYERS);
        hit_count++;
        return;
    }
#endif
    
    for (i = 0; i < game->num_players; i++)
    {
        if (game->players[i].rank != UNRANKED)
        {
            vector[i] = (game->num_players-1)*parameters.rank_bonus/2 - game->players[i].rank*parameters.rank_bonus;
        }
        else
        {
            vector[i] = evaluate_hand(game->players[i].hand);
            if (i == game->current_player)
            {
                vector[i] += parameters.current_player_bonus;
            }
        }
    }
    
    /* Normalise the vector somehow. */
    //TODO

#ifdef USE_TRANSPOSITION_TABLE
    memcpy(transposition_table[hash], vector, sizeof(int)*MAX_PLAYERS);
#endif
}


MOVE search_tree(GAME *game, int *best_vector, int to_depth, int *window_vector, int window_player)
{
    MOVE moves[MAX_MOVES];
    int num_moves;
    int i;
    MOVE best_move = MOVE_INVALID;
    int player;
    unsigned int hash;

    if (to_depth <= 0 || game_is_over(game))
    {
        evaluate_game_immediate(game, best_vector);
        return best_move;
    }

#ifdef USE_TRANSPOSITION_TABLE_SEARCH
    hash = hash_game(game) % TRANSPOSITION_TABLE_SIZE;

    if (transposition_table[hash][0] != TABLE_UNUSED)
    {
        memcpy(best_vector, transposition_table[hash], sizeof(int)*MAX_PLAYERS);
        hit_count++;
        return best_move;
    }
#endif
    
    num_moves = generate_valid_moves(game, moves);

    player = game->current_player;

    for (i = 0; i < num_moves; i++)
    {
        GAME *game2 = clone_game(game);
        int vector[MAX_PLAYERS];
        MOVE move;

        apply_move(game2, moves[i]);
        node_count++;
        move = search_tree(game2, vector, to_depth - 1, best_vector, player);
        if (move == MOVE_INVALID)
            move = moves[i];

        if (i == 0 || vector[player] > best_vector[player])
        {
            memcpy(best_vector, vector, sizeof(vector));
            best_move = moves[i];
        }

        stack_pop(&game_stack, game2);
        
        /* Prune if this move is so good for the current player that
           the potential value for the window player is less than
           the window. */
        if (vector[player] > parameters.total_score - window_vector[window_player])
        {
            break;
        }
    }

#ifdef USE_TRANSPOSITION_TABLE_SEARCH
    memcpy(transposition_table[hash], best_vector, sizeof(int)*MAX_PLAYERS);
#endif

    return best_move;
}


void evaluate_game(GAME *game, int *vector, int to_depth)
{
    int window[MAX_PLAYERS];
    
    memset(window, 0, sizeof(window));
    search_tree(game, vector, to_depth, window, game->current_player);
}


void evaluate_move(GAME *game, MOVE move, int *vector, int to_depth)
{
    GAME *game2 = clone_game(game);
    int window[MAX_PLAYERS];
    
    memset(window, 0, sizeof(window));
    apply_move(game2, move);
    search_tree(game2, vector, to_depth, window, game2->current_player);

    stack_pop(&game_stack, game2);
}


MOVE choose_move(GAME *game, int *best_vector, int to_depth)
{
    int window[MAX_PLAYERS];
    
    memset(window, 0, sizeof(window));    
    return search_tree(game, best_vector, to_depth, window, game->current_player);
}


void test(void)
{
    GAME game;
    MOVE moves[MAX_MOVES];
    int vector[MAX_PLAYERS];
    int num_moves;
    int i;
    MOVE chosen_move;
    clock_t start_time, end_time;
    int centiseconds, rate;

    printf("Rankoids AI test\n");

    clear_transposition_table();
    init_stack(&game_stack, 100*sizeof(GAME));

    initialise_game(&game);
    game.num_players = 3;
    game.players[0].hand[0] = 1;
    game.players[0].hand[1] = 3;
    game.players[0].hand[3] = 2;
    game.players[1].hand[4] = 1;
    game.players[0].hand[6] = 4;
    game.players[0].hand[7] = 3;
    game.players[0].hand[10] = 2;
    game.players[0].hand[JOKER_VALUE] = 1;
    game.players[1].hand[1] = 1;
    game.players[1].hand[2] = 3;
    game.players[1].hand[3] = 1;
    game.players[1].hand[4] = 1;
    game.players[1].hand[5] = 1;
    game.players[1].hand[9] = 3;
    game.players[1].hand[12] = 1;
    game.players[1].hand[5] = 2;
    game.players[2].hand[0] = 3;
    game.players[2].hand[2] = 1;
    game.players[2].hand[3] = 1;
    game.players[1].hand[4] = 2;
    game.players[2].hand[5] = 2;
    game.players[2].hand[9] = 1;
    game.players[2].hand[11] = 4;
    game.current_player = 0;
    game.pile_owner = 0;

    printf("Current game:\n");
    print_game(&game);

    printf("All moves for player 1:\n");
    num_moves = generate_all_moves(game.players[0].hand, moves);
    for (i = 0; i < num_moves; i++)
        print_move(moves[i]);
    printf("\n");

    evaluate_game_immediate(&game, vector);
    printf("Game vector is: [%d,%d,%d]\n", vector[0], vector[1], vector[2]);

    printf("All valid moves for player 1:\n");
    num_moves = generate_valid_moves(&game, moves);
    for (i = 0; i < num_moves; i++)
        print_move(moves[i]);
    printf("\n");

    chosen_move = MAKE_MOVE(3, 2);
    printf("Apply move ");
    print_move(chosen_move);
    printf(", game is:\n");
    apply_move(&game, chosen_move);
    print_game(&game);

    evaluate_game_immediate(&game, vector);
    printf("Game vector is: [%d,%d,%d]\n", vector[0], vector[1], vector[2]);

    printf("All valid moves for player 2:\n");
    num_moves = generate_valid_moves(&game, moves);
    for (i = 0; i < num_moves; i++)
    {
        print_move(moves[i]);
        clear_transposition_table();
        evaluate_move(&game, moves[i], vector, parameters.depth);
        printf(", with vector [%d,%d,%d]\n", vector[0], vector[1], vector[2]);
    }

    node_count = 0;
    clear_transposition_table();

    start_time = clock();
    chosen_move = choose_move(&game, vector, parameters.depth+5);
    end_time = clock();
    centiseconds = (end_time - start_time)*100 / CLOCKS_PER_SEC;
    if (centiseconds != 0)
    {
        rate = (int) (100.0*node_count/centiseconds);
    }
    else
    {
        rate = 0;
    }
    printf("%d nodes examined (%d hits), time was: %0.2f seconds, rate is: %d nodes/sec\n", node_count, hit_count, centiseconds/100.0, rate);
    printf("Chosen move was:");
    print_move(chosen_move);
    printf(", with vector [%d,%d,%d]\n", vector[0], vector[1], vector[2]);

    free_stack(&game_stack);
}
