% Solution to the "Einstein Puzzle", described at http://i.imgur.com/CPW4F.jpg.

% Is Sol a valid solution?  (Assumes 5 distinct origins, 5 distinct colours, 5 distinct drinks, 5 distinct pets, and 5 distinct cigarettes].
valid(Sol) :-
    Supply = [[english, german, norwegian, swedish, danish],
              [red, green, blue, yellow, white],
              [tea, coffee, milk, water, bier],
              [cats, dogs, horses, fish, birds],
              [blend, dunhill, pallmall, bluemasters, prince]],
    valid(Sol, Supply).

% Is [House|Rest] a distinct solution, given the supplies?  Supply is a list of lists, each of which is the values for a field.
% N.B. Base case only checks that first supply list is empty (remainder are assumed to be same length as first).
valid([], [[]|_]).
valid([House|Tail], Supply) :-
    valid_house(House, Supply, Remaining),
    valid(Tail, Remaining).

% Is [Cell|CellTail] a valid house, given supplies [Supply|SupplyTail] and leaving [Remaining|RemainingTail] as unused?
valid_house([], [], []).
valid_house([Cell|CellTail], [Supply|SupplyTail], [Remaining|RemainingTail]) :-
    select(Cell, Supply, Remaining),
    valid_house(CellTail, SupplyTail, RemainingTail).


% Create an empty solution template (a 5x5 list of lists, with all cells unrelated variables).
template([R1, R2, R3, R4, R5]) :-
    template_row(R1),
    template_row(R2),
    template_row(R3),
    template_row(R4),
    template_row(R5).

template_row([_, _, _, _, _]).


% The Englishman lives in the red house.
rule1(Template) :-
    member([english, red, _, _, _], Template).

% The Swede keeps dogs.
rule2(Template) :-
    member([swedish, _, _, dogs, _], Template).

% The Dane drinks tea.
rule3(Template) :-
    member([danish, _, tea, _, _], Template).
    
% The green house is just to the left of the white one.
rule4(Template) :-
    nth1(N1, Template, [_, green, _, _, _]),
    nth1(N2, Template, [_, white, _, _, _]),
    Diff is N2 - N1,
    Diff = 1.
    
% The owner of the green house drinks coffee.
rule5(Template) :-
    member([_, green, coffee, _, _], Template).
    
% The Pall Mall smoker keeps birds.
rule6(Template) :-
    member([_, _, _, birds, pallmall], Template).
    
% The owner of the yellow house smokes Dunhills.
rule7(Template) :-
    member([_, yellow, _, _, dunhill], Template).
    
% The man in the centre house drinks milk.
rule8(Template) :-
    Template = [_, _, [_, _, milk, _, _], _, _].
    
% The Norwegian lives in the first house.
rule9(Template) :-
    Template = [[norwegian, _, _, _, _], _, _, _, _].

% The blend smoker has a neighbour who keeps cats.
rule10(Template) :-
    nth1(N1, Template, [_, _, _, _, blend]),
    nth1(N2, Template, [_, _, _, cats, _]),
    Diff is N2 - N1,
    member(Diff, [-1, 1]).

% The man who smokes Blue Masters drinks bier.
rule11(Template) :-
    member([_, _, bier, _, bluemasters], Template).

% The man who keeps horses lives next to the Dunhill smoker.
rule12(Template) :-
    nth1(N1, Template, [_, _, _, horses, _]),
    nth1(N2, Template, [_,_,_,_, dunhill]),
    Diff is N2 - N1,
    member(Diff, [-1, 1]).

% The German smokes Prince.
rule13(Template) :-
    member([german, _, _, _, prince], Template).

% The Norwegian lives next to the blue house.
rule14(Template) :-
    nth1(N1, Template, [norwegian, _, _, _, _]),
    nth1(N2, Template, [_,blue, _, _, _]),
    Diff is N2 - N1,
    member(Diff, [-1, 1]).

% The blend smoker has a neighbour who drinks water.
rule15(Template) :-
    nth1(N1, Template, [_, _, _, _, blend]),
    nth1(N2, Template, [_, _, water, _, _]),
    Diff is N2 - N1,
    member(Diff, [-1, 1]).


is_rule(Rule) :-
    member(Rule, [rule9, rule14, rule8, rule1, rule2, rule3, rule4, rule5, rule6, rule7, rule10, rule11, rule12, rule13, rule15]).


solve(Template) :-
    findall(Rule, is_rule(Rule), Rules),
    reset_counter(apps),
    apply_rules(Template, Rules).


apply_rules(_, []).
apply_rules(Template, [Rule|Tail]) :-
    increment_counter(apps),
    apply(Rule, [Template]),
    apply_rules(Template, Tail).


% Counter to record number of operations performed.
:- dynamic(counter/2).

reset_counter(Name) :-
    retractall(counter(Name, _)),
    assertz(counter(Name, 0)),
    !.
increment_counter(Name) :-
    (counter(Name, N0), ! ; N0 = 0),
    N1 is N0 + 1,
    retractall(counter(Name, _)),
    assertz(counter(Name, N1)),
    !.


% Print the template [Line|Tail].
% Output form is x,...,z
%                y,...,w
% Unbound variable cells are rendered as _.
print_template([]) :-
    nl.
print_template([Line|Tail]) :-
    print_template_line(Line),
    print_template(Tail).

print_template_line([]) :-
    nl.
print_template_line([Cell]) :-
    print_template_cell(Cell),
    nl.
print_template_line([Cell,Cell2|Tail]) :-
    print_template_cell(Cell),
    write(','),
    print_template_line([Cell2|Tail]).

print_template_cell(Cell) :-
    var(Cell),
    write('_').
print_template_cell(Cell) :-
    \+var(Cell),
    write(Cell).
