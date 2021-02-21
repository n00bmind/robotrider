<pre>

                                              
                                   _         _         _    _           
                         ___  ___ | |_  ___ | |_  ___ |_| _| | ___  ___ 
                        |  _|| . || . || . ||  _||  _|| || . || -_||  _|
                        |_|  |___||___||___||_|  |_|  |_||___||___||_|  
                                                

                                                                        
</pre>

This is a (very work in progress) game engine & game where I get to experiment with a lot of game development ideas and rediscover the joy of coding along the way.

## Features

· Totally isolated platform / renderer / game layers (only Windows platform & OpenGL for now, there's also a PS4 branch which is private for obvious reasons)<br/>
· Hot code reloading (any change in the game layer code is immediately applied live)<br />
· Looped live code editing a la Handmade Hero (press F1 and input will start recording, press F1 again and whatever was recorded will be played back in a loop.. you can still modify the code live as per the previous point!)<br />
· Live shaders recompilation (a la ShaderToy)<br/>
· Debug mode (tilde) & editor mode (Ctrl+tilde)<br/>
· Infinite procedural world (WIP)<br/>
· Minimal dependencies (only ImGui and stb_image so far)<br/>
· Pure C++ without all the OOP crap (a.k.a. "C+")<br/>


## References
A list of papers, articles, sites, etc. that have been useful so far during development:

Marching cubes <br/>
http://paulbourke.net/geometry/polygonise/ <br/>
http://alphanew.net/index.php?section=articles&site=marchoptim&lang=eng

Fast quadric mesh simplification <br/>
https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification

Resampling meshes using MC <br/>
http://vcg.isti.cnr.it/publications/papers/mi_smi01.pdf

Wave Function Collapse <br/>
https://github.com/mxgmn/WaveFunctionCollapse <br/>
https://adamsmith.as/papers/wfc_is_constraint_solving_in_the_wild.pdf

Dual Contouring <br/>
https://www.cse.wustl.edu/~taoju/research/dualContour.pdf <br/>
https://people.eecs.berkeley.edu/~jrs/meshpapers/SchaeferWarren2.pdf <br/>
https://www.boristhebrave.com/2018/04/15/dual-contouring-tutorial/

QEF Minimization <br/>
https://graphics.rwth-aachen.de/media/papers/308/probabilistic-quadrics.pdf <br/>
https://github.com/nickgildea/qef <br/>



## Zen of Programming

After many many years dedicated to the beautiful art of programming (or 'coding' as I prefer to call it), I've grown more and more frustrated with the way software is conceived these days, what the industry considers 'best practices' and the path software development has taken in general. From a philosophical standpoint I believe this actually merely reflects the sad state of the modern world at large, but that's certainly a huge topic for another time.

In the last few years, however, thanks to the internet, I've discovered I'm not alone in thinking so, and have introduced myself to the ideas of some great minds out there, people like Casey Muratori, Jonathan Blow, Mike Acton, Sean Barrett, Brian Will, Omar Ocornut and others (to all of them, my most sincere thanks!). They showed me there is indeed another way, and above all, that this way is actually *superior*. This project is my first serious attempt to put some of these ideas into practice, to see where they take me.

With time, I found some of these ideas I actually knew already, I had only be taught very early on to forget about that stuff and write yet another stupid class hierarchy instead. Now I've seriously rediscovered the joy of programming, and why I started doing this in the first place. And so, just for fun, and also to make sure I don't forget these ideas ever again, I've decided to write down my very own

<br/>

######:: ZEN OF PROGRAMMING ::

 - Complexity is the enemy
 - Quality is a metric
 - Beauty matters
 - Code is for people, not machines
 - Your dependencies will enslave you
 - Your turnaround is way too long
 - Dumb code is very easy to debug
 - Marrying data and behaviour is like marrying Heaven and Hell
 - C++ is just evil
 - When tempted to abstract away, think twice
 - When tempted to design for the future, don't
 - When in doubt, make it explicit
 - Don't be afraid of long functions
 - Focus. Pay attention. The devil is in the details


