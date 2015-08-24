Somewhere to put my meagre contributions to free software.  It beats sticking source files on Pastebin... ;-)

I have been programming since about 1990.  My first program (made with the help of my stepfather Ross) was something like:

```
10 INPUT "What is your name",N$
20 IF TIME$ < "12:00" THEN PRINT "Good morning,";N$ : GOTO 60
30 IF TIME$ < "18:00" THEN PRINT "Good afternoon,";N$ : GOTO 60
40 IF TIME$ < "21:00" THEN PRINT "Good evening,";N$ : GOTO 60
50 IF TIME$ > "21:00" THEN PRINT "Good night,";N$;"!" : GOTO 60
60 GOTO 10
```

What happens at 9 o'clock sharp?  Who knows!  Ah, the mysterious, deterministic and unpredictable computer.  I've confabulated that particular bug; but I'm pretty sure there must have been several in a program that large.

At its peak my Subversion repository had hundreds of projects of various size and states of completion (though tending towards nascency).  Unfortunately I lost it all in a house fire in November 2010.  Including the backups.

So I've been slowly building up again, starting with the few things I salvaged.  If I upload things here then more of my creations may survive the next inevitable calamity. :-)

Below is a list of things I plan to upload here.  Included are some links to blog posts about them at http://ejrh.wordpress.com/.

# Contents #

The items have been triaged into three general categories:
  * Hopefully useful - written to get work done, and have had some effort put in to make them general purpose and/or reliable and/or easy to use.
  * Possibly interesting - things I was working on/playing with for my own amusement/edification.
  * Everything else - things that aren't especially sensitive except insofar as they reveal my programming limitations.  Ah, what's the worst that could happen!

## Hopefully useful ##

**Project 6014** (in collaboration with several others) - a mod for Star Control 2.

Location: http://code.google.com/p/project6014/

Information:
  * http://ejrh.wordpress.com/2011/03/11/star-control-ii-mod/
  * http://ejrh.wordpress.com/2011/05/17/sc2-mod-next-steps/
  * http://ejrh.wordpress.com/2011/05/20/sc2-mod-design-notes/

**xmlgrep** - Grep XML files using XPath expressions.

Location: tools/xmlgrep

Information: http://ejrh.wordpress.com/2011/05/10/xml-grep/

**replay** - Replay Subversion revisions (less strict than svnsync).

Location: tools/replay

Information: TODO

**network** - A clone of KNetwalk.

Location: games/network

Information:
  * http://ejrh.wordpress.com/2011/04/26/ai-for-a-logic-game/
  * http://ejrh.wordpress.com/2011/05/12/depth-first-logic-game-ai/

**snowflake** - A simple "mindmap" editor.

Location: snowflake

Information: TODO

**emaildb** - A DB schema for email messages, with import and export scripts for MBOX files.

Location: emaildb

Information: http://ejrh.wordpress.com/2011/08/21/e-mail-recovery/


## Possibly interesting ##

**quantcup** - My entry to QuantCup Challange 1.

Location: quantcup

Information:
  * http://ejrh.wordpress.com/2011/04/21/price-time-matching-engine/
  * http://ejrh.wordpress.com/2011/05/17/quantcup-update/
  * http://ejrh.wordpress.com/2011/05/21/sock-matching-engine/

**fractals** - A program for drawing the Mandelbrot set.

Location: fractals

Information:
  * http://ejrh.wordpress.com/2011/04/15/fractals/
  * http://ejrh.wordpress.com/2011/04/16/not-a-julia-set/

**compiler** - A compiler for a C-like language, written in C.

Location: compiler

Information: TODO

**galaxy** - N-body force simulations.

Location: simulation/galaxy

Information:
  * http://ejrh.wordpress.com/2011/02/10/ephemerides-from-jpl-horizons/
  * http://ejrh.wordpress.com/2011/03/05/faking-relativity/
  * http://ejrh.wordpress.com/2011/06/26/barnes-hut-for-n-body-force-calculations/

**fs** - A user-mode file system, using B-trees.

Location: fs

Information:
  * http://ejrh.wordpress.com/2011/07/29/writing-a-file-system/
  * http://ejrh.wordpress.com/2011/08/09/directory-hard-links/

**polyhedra** - Python script for drawing Polyhedra tesselations, for printing.

Location: TODO

Information: http://ejrh.wordpress.com/2011/01/25/pure-paper-packable-polyhedra-in-python/

**music** - Synthesise music from discrete note information.

Location: music

Information: http://ejrh.wordpress.com/2011/04/02/music-synthesis/

## Everything else ##

Patch for Open Hardware Monitor - I'm still not sure how to version this one so it's still in Pastebin.

Location: http://pastebin.com/gUsdQbqR

Information:
  * http://ejrh.wordpress.com/2011/02/25/monitoring-hardware/
  * http://ejrh.wordpress.com/2011/04/11/gpu-temperature-control/
  * http://ejrh.wordpress.com/2011/05/13/open-hardware-monitor-logging-and-fan-control/