from music import NoteMixture, make_piano_melody, SAMPLE_RATE


def toccata():
    minim_length=2/6.0
    l1 = make_piano_melody("""
T< T< A1 G T> A1 , , T< G F E D T> C# T> D , , ,
T< T< A G-1 T> A , , E-1 F-1 C#-1 T> D-1 , , ,
T< T< A-1 G-2 T> A-1 , , T< G-2 F-2 E-2 D-2 T> C#-2 T> D-2 , , ,

    """, duration=minim_length)
    r1 = make_piano_melody("""
T< T< A2 G1 T> A2 , , T< G1 F1 E1 D1 T> C#1 T> D1 , , ,
T< T< A1 G T> A1 , , E F C# T> D , , ,
T< T< A G-1 T> A , , T< G-1 F-1 E-1 D-1 T> C#-1 T> D-1 , , ,
    """, duration=minim_length)
    
    n = NoteMixture()
    n.append_note(l1, offset=0)
    n.append_note(r1, offset=0)
    return n.collapse().trim()


def fugue():
    minim_length=4/6.0
    bar_length = minim_length*4
    
    right = []
    left = []
    pedal = []
    
    #0 - 0
    right.append("""
, , , , , , , , T< T< , D1 C1 D1 Bb1 D1 A1 D1
""")
    left.append("""
T< T< , A1 G A1 F A1 E A1
D A1 C# A1 D A1 E A1 F A1 A A1 B A1 C# A1
D A1 C# A1 D A1 E A1 T> F F# G C
""")
    pedal.append(""", , , , , , , , , ,""")

    #1 - 3
    right.append("""
T< T< G D1 F# D1 G D1 A1 D1 Bb1 D1 D D1 E D1 F# D1
G D1 F# D1 G D1 A1 D1 T> Bb1 D1 Bb1 D1
Eb1 G Eb1 G C1 A1 C1 A1
""")
    left.append("""
T< Bb A Bb C D F#-1 G-1 A
Bb A Bb F#-1 T< G-1 G G-1 G D G D G
C Eb C Eb C Eb C Eb C F C F C F C F
""")
    pedal.append(""", , , , , , , , , , , ,""")

    #2 - 6
    right.append("""
T< D1 F D1 F Bb1 G Bb1 G
C#1 E C#1 E A1 F A1 F
G C# G C# F D F D
""")
    left.append("""
T< T< Bb D Bb D Bb D Bb D Bb E Bb E Bb E Bb E
A C# A C# A C# A C# F-1 D F-1 D F-1 D F-1 D
E-1 Bb E-1 Bb E-1 Bb E-1 Bb D-1 A D-1 A D-1 A D-1 A
""")
    pedal.append(""", , , , , , , , , , , ,""")

    #3 - 9
    right.append("""
T< E B E B T< , A2 G1 A2 F1 A2 E1 A2
D1 A2 C#1 A2 D1 A2 E1 A2 F1 A2 A1 A2 B1 A2 C#1 A2
D1 A2 C#1 A2 D1 A2 E1 A2 F1 A2 E1 A2 D1 A2 C1 A2
""")
    left.append("""
T< T< E-1 G-1 E-1 G-1 E-1 G-1 E-1 G-1 T> A A A A
A A A A A , A A
A A A A T> A D
""")
    pedal.append("""
, , T< F-1 E-1 D-1 G-1
F-1 E-1 F-1 C#-1 D-1 C#-1 D-1 E-1
F-1 E-1 F-1 C#-1 T> D-1 G-1
""")

    #4 - 12
    right.append("""
T< T< Bb1 A2 C1 A2 D1 G1 Bb1 G1 E1 G1 D1 G1 C1 G1 Bb1 G1
A1 G1 Bb1 G1 C1 F1 A1 F1 D1 F1 C1 F1 Bb1 F1 A1 F1
G F1 A1 F1 Bb1 E1 G E1 C#1 E1 Bb1 E1 A1 E1 G E1
""")
    left.append("""
D , C C
C , Bb Bb
Bb , A A
""")
    pedal.append("""
G-1 , C-1 E
F-1 , Bb-1 D-1
E-1 , A-1 C#-1
""")

    #5 - 15
    right.append("""
T< T< F E1 G E1 A1 D1 F D1 E E1 E E1 F D1 F D1
Bb1 C#1 Bb1 C#1 A1 D1 F D1 E E1 E E1 F D1 F D1
, D1 C#1 D1 B1 D1 C#1 B1 T> T> C#1 ,
""")
    left.append("""
A T< , D C# C# D D
T> E T< , D C# C# D D
T> E , T< T< , A1 G A1 E G F E
""")
    pedal.append("""
D-1 T< , F-1 Bb Bb A A
T> G-1 T< , A Bb Bb A A
T> G#-1 , A ,
""")

    #6 - 18
    right.append("""
T< T< , D1 C#1 D1 F1 D1 C#1 B1 T> T> C#1 E1
T< E1 D1 D1 C#1 T> C1 Bb1
A1 A1 G G
""")
    left.append("""
F D T< T< A A1 G A1 C# G F E
T> T> F E T< D A1 A1 G
T> G F# F Eb
""")
    pedal.append("""
, , , ,
, T< T< , A G-1 A F#-1 D C# D G-1 F E D
C# E A C# D-1 Eb D C B D G-1 B C-1 D C Bb
""")

    #7 - 21
    right.append("""
T< F# T> A1 T< Eb1 T> D1 T< , G1
G1 F#1 T> G1 T< Bb1 D1 D1 D1
T> D1 T< D1 D1 D1 D1 D1 D1 T>
T> T> + + + T< T<
T< Eb D F C1 C1 T< Bb1 A1 T> Bb1 Bb1
T> A1 T< D1 C1 Bb1 A1 Bb1 F#
G F# G A1 Bb1 A1 Bb1 F#
""") 
    left.append("""
T< T< A C F#-1 A D-1 C Bb A Bb A1 G F# G Bb A G-1
T> T> D T< , D D D D C
Bb D D C Bb D D C
""")
    pedal.append("""
, , , ,
T< T< , D-1 C-1 D-1 Bb-1 D-1 A-1 D-1 G-2 D-1 F#-2 D-1 G-2 D-1 A-1 D-1
Bb-1 D-1 D-2 D-1 E-2 D-1 F#-2 D-1 G-2 D-1 F#-2 D-1 G-2 D-1 A-1 D-1
""")
    
    #8 - 24
    right.append("""
T< T< D1 G1 F1 G1 E1 F1 D1 E1 C1 A2 G1 A2 F1 G1 E1 F1
D1 Bb2 A2 Bb2 G1 A2 F1 G1 E1 C2 Bb2 C2 A2 Bb2 G1 A2
F1 Eb1 D1 C1 D1 C1 Bb1 A1 Bb1 D1 Bb1 A1 G Bb1 G F
""") 
    left.append("""
T< T< G Bb1 A1 Bb1 G A1 F G E C1 Bb1 C1 A1 Bb1 G A1
F D1 C1 D1 Bb1 C1 A1 Bb1 G E1 D1 E1 C1 D1 Bb1 C1
T> A1 F T< Bb1 A1 G F G Bb1 G F Eb G Eb D
""")
    pedal.append("""
Bb-1 T< , B-1 T> C-1 T< , C#-1
T> D-1 T< , D-1 T> E-1 T< , E-1
F-1 A-1 Bb-1 D-1 T> G-2 T< , Bb-1""")

    #9 - 27
    right.append("""
T< T< E F G A1 Bb1 D1 C1 Bb1 T> T> A1 T< C1 Bb1
A1 G A1 Bb1 C1 E F G
A1 G A1 Bb1 T< C1 Bb1 A1 G F Eb D C
""")
    left.append("""
T< T< C D E F T> G E T< F C Bb C A C G-1 C
F-1 C E-1 C F-1 C G-1 C A C C-1 C D-1 C E-1 C
F-1 C E-1 C F-1 C G-1 C T> A , , ,""")
    pedal.append("""T< Bb-1 A-1 G-2 C-1 T> F-2 ,
, , , ,
, , , ,""")
    
    #10 - 30
    right.append("""
T< T< D1 C1 Bb1 A1 G F E D E1 D1 C1 Bb1 A1 G F E
F1 E1 D1 C1 Bb1 A1 G F G1 F1 E1 D1 C1 Bb1 A1 G
A2 F1 E1 F1 C1 F1 E1 F1 A2 F1 E1 F1 C1 F1 E1 F1
""")
    left.append("""
T< Bb , , , C , , ,
D , , , E , , ,
F , C1 , F , C1 ,
""")
    pedal.append(""", , , , , , , , , , , ,""")

    #11 - 33
    right.append("""
T< T< G1 E1 D1 E1 C1 E1 D1 E1 G1 E1 D1 E1 C1 E1 D1 E1
A2 F1 E1 F1 C1 F1 E1 F1 A2 F1 E1 F1 C1 F1 E1 F1
G1 E1 D1 E1 C1 E1 D1 E1 G1 E1 D1 E1 C1 E1 D1 E1""")
    left.append("""
T< E , C1 , E , C1 ,
F , C1 , F , C1 ,
E , C1 , E , C1 ,
""")
    pedal.append(""", , , , , , , , , , , ,""")

    #12 - 36
    right.append("""
T< T< F1 G1 F1 E1 D1 C1 B1 A1 B1 G B1 D1 F1 A2 F1 D1
B1 G B1 D1 F1 A2 F1 D1 Bb1 G Bb1 C1 E1 G1 E1 C1
Bb1 G Bb1 C1 E1 G1 E1 C1 A1 F A1 C1 D1 F1 D1 Bb1
""")
    left.append("""
T< D , T> , , , , , , , , , , ,
""")
    pedal.append(""", , , , , , , , , , , ,""")

    #13 - 39
    right.append("""
T< T< A1 F A1 C1 D1 F1 D1 Bb1 G E G Bb1 C#1 E1 C#1 Bb1
G E G Bb1 C#1 E1 C#1 Bb1 , A2 G1 A2 F1 A2 E1 A2
D1 A2 C#1 A2 D1 A2 E1 A2 F1 A2 A1 A2 B1 A2 C#1 A2
""")
    left.append(""", , , ,
, , T< A1 C#1 D1 G
F A1 B1 C#1 D1 C#1 D1 E1
""")
    pedal.append(""", , , , , , , , , , , ,""")

    #14 - 42
    right.append("""
T< T< F1 A2 E1 A2 T> F1 E1 D1 C1 Bb1 A1
T< Bb1 A1 G F# E D C# B T> T> , , , , , ,
T> T> + + + T< T<
T< D1 C#1 D1 C#1 D1 A1 G F# G , T> , , , , , , ,
""")
    left.append("""
, T< T< , A1 G A1 , F# E F# , D C D
Bb C Bb A G-1 F-1 E-1 D-1 C#-1 A-1 C#-1 E-1 G-1 Bb G-1 E-1
C#-1 A-1 C#-1 E-1 G-1 Bb G-1 E-1 D-1 A-1 D-1 F-1 A D A F-1
""")
    pedal.append(""", , , , , , , , , , , ,""")

    #15 - 45
    right.append(""", , , , , , , , , , , ,""")
    left.append("""
T< T< D-1 A-1 D-1 F-1 A D A F-1 C#-1 A-1 C#-1 E-1 G-1 Bb G-1 E-1
C#-1 A-1 C#-1 E-1 G-1 Bb G-1 E-1 D-1 A-1 D-1 F-1 A D A F-1
D-1 A-1 D-1 F-1 A D A F-1 E-1 C#-1 E-1 G-1 Bb C# Bb G-1
""")
    pedal.append(""", , , , , , , , , , , ,""")

    #16 - 48
    right.append(""", , , , , , , , , , , ,""")
    left.append("""
T< T< E-1 C#-1 E-1 G-1 Bb C# Bb G-1 F-1 D-1 F-1 A D F D A
F-1 D-1 F-1 A D F D A E-1 C#-1 E-1 G-1 Bb C# Bb G-1
E-1 C#-1 E-1 G-1 Bb C# Bb G-1 F-1 D-1 F-1 A D F D A
""")
    pedal.append(""", , , , , , , , , , , ,""")

    #17 - 51
    right.append(""", , , , , , , , , , , ,""")
    left.append("""
T< T< F-1 D-1 F-1 A D F D A G-1 E-1 G-1 Bb C# E C# Bb
G-1 E-1 G-1 Bb C# E C# Bb A F-1 A C# D F D A
Bb D Bb G-1 F-1 A F-1 D-1 A-1 D-1 A-1 F-2 D-2 D-1 C#-1 B-1
""")
    pedal.append(""", , , , , , , , , , , ,""")

    #18 - 54
    right.append(""", , , , , T< T< , F E D A1 , , , , E1 D1 C#1""")
    left.append("""
T< T< C#-1 Bb A G-1 F-1 G-1 F-1 E-1 D-1 Bb A G-1 F-1 G-1 F-1 E-1
D-1 T< E-1 F-1 G-1 A B C# T> D , , , A T< B C# D E F G T> A1 G F E
""")
    pedal.append(""", , , , , , , ,""")

    #19 - 56
    right.append("""
T< T< D1 T< Eb1 D1 C#1 Bb1 A1 G T> T> F# T> A1 T< G C1 B1
Eb1 D1 Eb1 B1 C1 B1 C1 D1
""")
    left.append("""
T< F , T< , Eb D C T> Bb B C G
T< T< G F G F G F G F G F G F G F G F G F G F G F G F G F G F G F G F
""")
    pedal.append("""
, , T< T< , G-1 F-1 G-1 Eb-1 G-1 D-1 G-1
C-1 G-1 B-1 G-1 C-1 G-1 D-1 G-1 Eb-1 G-1 G-2 G-1 A-1 G-1 B-1 G-1
""")

    #20 - 58
    right.append("""
T< Eb1 D1 Eb1 F1 T< T< G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1
G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1
""")
    left.append("""
T< T< T< G F G F G F G F G F G F G F G F T> G G F G Eb G D G
C G B G C G D G Eb G G-1 G A G B G
""")
    pedal.append("""
T< T< C-1 G-1 B-1 G-1 C-1 G-1 D-1 G-1 T> Eb-1 D-1 C-1 B-1
C-1 D-1 Eb-1 F-1 G-1 B-1 C-1 D-1
""")

    #21 - 60
    right.append("""
T< T< T< G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 G1 F1 T> G1 G1 F1 G1 Eb1 F1 D1 Eb1
C1 F1 Eb1 F1 D1 Eb1 C1 D1 B1 Eb1 D1 Eb1 C1 D1 B1 C1
""")
    left.append("""
T< T< C G B G C G D G T> Eb D T< G G# F G
T> Eb C T< F G Eb F T> D B T< Eb F D Eb
""")
    pedal.append("""
T< Eb-1 D-1 Eb-1 F-1 G-1 B-1 C-1 D-1
Eb-1 A-1 B-1 C-1 D-1 G-2 A-1 B-1
""")

    #22 - 62
    right.append("""
T< T< A1 D1 C1 D1 Bb1 C1 A1 Bb1 G Bb1 A1 Bb1 C1 Bb1 A1 G
T> F# A1 T< D1 G C1 F# Bb1 G D1 A1 Bb1 G A1 F#
""")
    left.append("""
T< C F# T< G A1 F G Eb G F# G A1 G F# E
, D C D Bb D A D G-1 D F#-1 D G-1 D A-1 D
""")
    pedal.append("""
T< C-1 D-1 G-1 D-1 Eb-1 Bb-1 A-1 C-1
T> T> T> D-1
""")

    #23 - 64
    right.append("""
T< T< G0 D1 F#0 D1 G0 D1 A1 D1 Bb1 G0 D1 G0 Bb1 G0 C1 G0

""")
    left.append("""
""")
    pedal.append("""
T> T> T> D-1
T< T< T< D-1 , T< G-2 C-1 Ab-1 D-1 D-2
""")

    n = NoteMixture()
    for series in [right, left, pedal]:
        mix = NoteMixture()
        #series = series[21:]
        for line in series:
            line_note = make_piano_melody(line, duration=minim_length)
            minim_count = line_note.length/SAMPLE_RATE/minim_length
            if minim_count % 4 != 0:
                print "Warning: bar length is: %d minims" % minim_count
            mix.append_note(line_note)
        n.append_note(mix, offset=0)
    
    return n.collapse().trim()


if __name__ == '__main__':
    fugue().play()
