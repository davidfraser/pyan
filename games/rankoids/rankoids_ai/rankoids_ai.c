#include "Python.h"

#include "ai.h"


static PyObject *
test_method(PyObject *self, PyObject *args)
{
	test();

	/* Return Python None. */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *
set_method(PyObject *self, PyObject *args)
{
	const char *param;
	int value;
	if (!PyArg_ParseTuple(args, "si", &param, &value))
		return NULL;

	if (!strcmp(param, "depth"))
		parameters.depth = value;
	else if (!strcmp(param, "current_player_bonus"))
		parameters.current_player_bonus = value;
	else if (!strcmp(param, "hand_size_bonus"))
		parameters.hand_size_bonus = value;
	else if (!strcmp(param, "different_card_bonus"))
		parameters.different_card_bonus = value;
	else if (!strcmp(param, "rank_bonus"))
		parameters.rank_bonus = value;
	else
	{
		PyErr_SetString(PyExc_KeyError, "No parameter with that name");
		return NULL;
	}

	/* Return Python None. */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *
get_method(PyObject *self, PyObject *args)
{
	const char *param;
	int value;
	if (!PyArg_ParseTuple(args, "s", &param))
		return NULL;

	if (!strcmp(param, "depth"))
		value = parameters.depth;
	else if (!strcmp(param, "current_player_bonus"))
		value = parameters.current_player_bonus;
	else if (!strcmp(param, "hand_size_bonus"))
		value = parameters.hand_size_bonus;
	else if (!strcmp(param, "different_card_bonus"))
		value = parameters.different_card_bonus;
	else if (!strcmp(param, "rank_bonus"))
		value = parameters.rank_bonus;
	else
	{
		PyErr_SetString(PyExc_KeyError, "No parameter with that name");
		return NULL;
	}

	/* Return value as Python object. */
	return Py_BuildValue("i", value);
}


static PyObject *
list_method(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	return Py_BuildValue("[s,s,s,s,s]",
		"depth",
		"current_player_bonus",
		"hand_size_bonus",
		"different_card_bonus",
		"rank_bonus");
}


static GAME game;


static PyObject *
init_game_method(PyObject *self, PyObject *args)
{
	int num_players;
	int current_player;
	int pile_owner;
	int pile_value;
	int pile_count;
	if (!PyArg_ParseTuple(args, "iii(ii)", &num_players, &current_player, &pile_owner, &pile_value, &pile_count))
		return NULL;

	initialise_game(&game);
	game.num_players = num_players;
	game.current_player = current_player;
	game.pile_owner = pile_owner;
	game.pile = MAKE_MOVE(pile_value, pile_count);

	/* Return Python None. */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *
rank_player_method(PyObject *self, PyObject *args)
{
	int player;
	if (!PyArg_ParseTuple(args, "i", &player))
		return NULL;

	game.players[player].rank = game.next_rank++;

	/* Return Python None. */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *
add_card_method(PyObject *self, PyObject *args)
{
	int player;
	int value;
	if (!PyArg_ParseTuple(args, "ii", &player, &value))
		return NULL;

	game.players[player].hand[value]++;

	/* Return Python None. */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *
evaluate_game_method(PyObject *self, PyObject *args)
{
	int vector[MAX_PLAYERS];
	PyObject *v;
	int i;

	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	clear_transposition_table();
	init_stack(&game_stack, 100*sizeof(GAME));
	node_count = 0;
	hit_count = 0;
	evaluate_game(&game, vector, parameters.depth);
	free_stack(&game_stack);

	//printf("nodes = %d, hits = %d\n", node_count, hit_count);

	/* Return Python list for vector. */
	v = PyList_New(game.num_players);
	if (v == NULL)
		return NULL;
	for (i = 0; i < game.num_players; i++)
	{
		PyObject *w = PyInt_FromLong(vector[i]);
		if (w == NULL) {
			Py_DECREF(v);
			return NULL;
		}
		PyList_SET_ITEM(v, i, w);
	}
	return v;
}


static PyObject *
evaluate_move_method(PyObject *self, PyObject *args)
{
	int value;
	int count;
	MOVE move;
	int vector[MAX_PLAYERS];
	PyObject *v;
	int i;

	if (!PyArg_ParseTuple(args, "(ii)", &value, &count))
		return NULL;

	move = MAKE_MOVE(value, count);

	clear_transposition_table();
	init_stack(&game_stack, 100*sizeof(GAME));
	node_count = 0;
	hit_count = 0;
	evaluate_move(&game, move, vector, parameters.depth);
	free_stack(&game_stack);

	//printf("nodes = %d, hits = %d\n", node_count, hit_count);

	/* Return Python list for vector. */
	v = PyList_New(game.num_players);
	if (v == NULL)
		return NULL;
	for (i = 0; i < game.num_players; i++)
	{
		PyObject *w = PyInt_FromLong(vector[i]);
		if (w == NULL) {
			Py_DECREF(v);
			return NULL;
		}
		PyList_SET_ITEM(v, i, w);
	}
	return v;
}


static PyObject *
choose_move_method(PyObject *self, PyObject *args)
{
	MOVE move;
	int value;
	int count;
	int vector[MAX_PLAYERS];

	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	clear_transposition_table();
	init_stack(&game_stack, 100*sizeof(GAME));
	node_count = 0;
	move = choose_move(&game, vector, parameters.depth);
	free_stack(&game_stack);

	value = MOVE_VALUE(move);
	count = MOVE_COUNT(move);

	/* Return Python tuple representing move. */
	return Py_BuildValue("(ii)", value, count);
}


static PyObject *
print_game_method(PyObject *self, PyObject *args)
{
	int i;
	MOVE moves[MAX_MOVES];
	int num_moves;

	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	print_game(&game);
	num_moves = generate_valid_moves(&game, moves);
	for (i = 0; i < num_moves; i++)
		print_move(moves[i]);
	printf("\n");


	/* Return Python None. */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyMethodDef module_methods[] = {
	{"test", test_method, METH_VARARGS, "Test the Rankoids AI."},
	{"set", set_method, METH_VARARGS, "Set an AI parameter."},
	{"get", get_method, METH_VARARGS, "Get an AI parameter."},
	{"list", list_method, METH_VARARGS, "List all AI parameters."},
	{"init_game", init_game_method, METH_VARARGS, "Initialise an AI game state with number of players, current player, pile owner, and pile."},
	{"rank_player", rank_player_method, METH_VARARGS, "Rank a player in the AI game state."},
	{"add_card", add_card_method, METH_VARARGS, "Give a card to a player in the AI game state."},
	{"evaluate_game", evaluate_game_method, METH_VARARGS, "Run the AI, returning the value of this game state."},
	{"evaluate_move", evaluate_move_method, METH_VARARGS, "Run the AI, returning the value of this move."},
	{"choose_move", choose_move_method, METH_VARARGS, "Run the AI, returning the best move."},
	{"print_game", print_game_method, METH_VARARGS, "Print the AI game state."},
	{NULL, NULL}
};


PyMODINIT_FUNC
initrankoids_ai(void)
{
	Py_InitModule("rankoids_ai", module_methods);
}
