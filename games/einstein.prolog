% Solution to the "Einstein Puzzle", described at http://i.imgur.com/CPW4F.jpg.

% Is Sol a valid solution?  (Assumes 5 distinct origins, 5 distinct colours, 5 distinct drinks, 5 distinct pets, and 5 distinct cigarettes].
valid(Sol) :-
    Supply = [[english, german, norwegian, swedish, danish], [red, green, blue, yellow, white], [tea, coffee, milk, water, bier], [cats, dogs, horses, fish, birds], [blend, dunhill, pallmall, bluemasters, prince]],
    valid(Sol, Supply).

% Is [House|Rest] a distinct solution, given the supplies of origins/colours,drinks,pets,cigarettes?
valid([], [[]|_]).
valid([House|Rest], Supply) :-
    valid_house(House, Supply, Remaining),
    valid(Rest, Remaining).

% Is [F|OtherF] a valid house, given supplies [S|OtherS] and leaving [R|OtherR] as unused?
valid_house([], [], []).
valid_house([F|OtherF], [S|OtherS], [R|OtherR]) :-
    select(F, S, R),
    valid_house(OtherF, OtherS, OtherR).


template([R1, R2, R3, R4, R5]) :-
    template_row(R1),
    template_row(R2),
    template_row(R3),
    template_row(R4),
    template_row(R5).

template_row([_, _, _, _, _]).


% The Englishman lives in the red house.
action1(T) :-
    member([english, red, _, _, _], T).

% The Swede keeps dogs.
action2(T) :-
    member([swedish, _, _, dogs, _], T).

% The Dane drinks tea.
action3(T) :-
    member([danish, _, tea, _, _], T).
    
% The green house is just to the left of the white one.
action4(T) :-
    nth1(N, T, [_, green, _, _, _]),
    nth1(N2, T, [_, white, _, _, _]),
    D is N2-N,
    D = 1.
    
% The owner of the green house drinks coffee.
action5(T) :-
    member([_, green, coffee, _, _], T).
    
% The Pall Mall smoker keeps birds.
action6(T) :-
    member([_, _, _, birds, pallmall], T).
    
% The owner of the yellow house smokes Dunhills.
action7(T) :-
    member([_, yellow, _, _, dunhill], T).
    
% The man in the centre house drinks milk.
action8(T) :-
    T = [_, _, [_, _, milk, _, _], _, _].
    
% The Norwegian lives in the first house.
action9(T) :-
    T = [[norwegian, _, _, _, _], _, _, _, _].

% The blend smoker has a neighbour who keeps cats.
action10(T) :-
    nth1(N, T, [_, _, _, _, blend]),
    nth1(N2, T, [_, _, _, cats, _]),
    D is N2-N,
    member(D, [-1,1]).

% The man who smokes Blue Masters drinks bier.
action11(T) :-
    member([_, _, bier, _, bluemasters], T).

% The man who keeps horses lives next to the Dunhill smoker.
action12(T) :-
    nth1(N, T, [_, _, _, horses, _]),
    nth1(N2, T, [_,_,_,_, dunhill]),
    D is N2-N,
    member(D, [-1,1]).

% The German smokes Prince.
action13(T) :-
    member([german, _, _, _, prince], T).

% The Norwegian lives next to the blue house.
action14(T) :-
    nth1(N, T, [norwegian, _, _, _, _]),
    nth1(N2, T, [_,blue, _, _, _]),
    D is N2-N,
    member(D, [-1,1]).

% The blend smoker has a neighbour who drinks water.
action15(T) :-
    nth1(N, T, [_, _, _, _, blend]),
    nth1(N2, T, [_, _, water, _, _]),
    D is N2-N,
    member(D, [-1,1]).


solve(T) :-
    action1(T),
    action2(T),
    action3(T),
    action4(T),
    action5(T),
    action6(T),
    action7(T),
    action8(T),
    action9(T),
    action10(T),
    action11(T),
    action12(T),
    action13(T),
    action14(T),
    action15(T).


% The bound values of X dont occur in [H|T].
not_in(_, []).
not_in(X, [H|T]) :-
    not_eq(X, H),
    not_in(X, T).

% The bound values of X are distinct from the bound values of Y.
not_eq([], []).
not_eq([H1|T1], [H2|T2]) :- var(H1), not_eq(T1, T2), !.
not_eq([H1|T1], [H2|T2]) :- var(H2), not_eq(T1, T2), !.
not_eq([H1|T1], [H2|T2]) :- H1 \= H2, not_eq(T1, T2), !.



% Print the template [H|T].
print_template([]) :-
    nl.
print_template([H|T]) :-
    print_template_line(H),
    print_template(T).

print_template_line([]) :-
    nl.
print_template_line([X]) :-
    print_template_cell(X),
    nl.
print_template_line([H,H2|T]) :-
    print_template_cell(H),
    write(','),
    print_template_line([H2|T]).

print_template_cell(X) :-
    var(X),
    write('_').
print_template_cell(X) :-
    \+var(X),
    write(X).
