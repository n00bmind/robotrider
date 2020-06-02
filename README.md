<pre>

                                              
           _         _         _    _           
 ___  ___ | |_  ___ | |_  ___ |_| _| | ___  ___ 
|  _|| . || . || . ||  _||  _|| || . || -_||  _|
|_|  |___||___||___||_|  |_|  |_||___||___||_|  
                                                

                                                                        
</pre>

This is a (very work in progress) game & game engine where I get to experiment with a lot of game development ideas and rediscover the joy of coding along the way.

## Features

· Totally isolated platform / renderer / game layers (only Windows platform & OpenGL for now)<br/>
· Hot code reloading (any change in the game layer code is immediately applied live)<br />
· Looped live code editing (press F1 and input will start recording, press F1 again and whatever was recorded will be played back in a loop.. you can still modify the code live as per the previous point!)<br />
· Live shaders recompilation (a la ShaderToy)<br/>
· Debug mode (tilde) & editor mode (Ctrl+tilde)<br/>
· Developer console with customizable commands<br />
· Infinite procedural world (WIP)<br/>
· Minimal dependencies (only ImGui and stb_image so far)
· Pure C++ without all the OOP crap



## References
A list of papers, articles, sites, etc. that have been useful during development:

Marching cubes :: http://paulbourke.net/geometry/polygonise/ <br/> & http://alphanew.net/index.php?section=articles&site=marchoptim&lang=eng <br/>
Fast quadric mesh simplification :: https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification <br/>
Resampling meshes using MC :: http://vcg.isti.cnr.it/publications/papers/mi_smi01.pdf <br/>
Wave Function Collapse :: https://github.com/mxgmn/WaveFunctionCollapse & https://adamsmith.as/papers/wfc_is_constraint_solving_in_the_wild.pdf <br/>
Dual Contouring :: https://www.cse.wustl.edu/~taoju/research/dualContour.pdf & https://people.eecs.berkeley.edu/~jrs/meshpapers/SchaeferWarren2.pdf <br/> https://www.boristhebrave.com/2018/04/15/dual-contouring-tutorial/ <br/>
QEF Minimization :: https://graphics.rwth-aachen.de/media/papers/308/probabilistic-quadrics.pdf & https://github.com/nickgildea/qef



## Zen of Programming

After many many years dedicated to the ultimate creative endeavour, the beautiful art of programming (or 'coding' as I prefer to call it), I've grown more and more frustrated with the way software is conceived these days, what the industry considers 'best practices' and the path software development has taken. From a phylosophical standpoint I believe this actually merely reflects the sad state of the modern world at large, but that's certainly a huge topic for another time.

In the last few years, however, thanks to the internet, I've discovered I'm not alone in thinking so, and have introduced myself to the ideas of some great minds out there, people like Casey Muratori, Jonathan Blow, Mike Acton, Sean Barrett, Brian Will, Omar Ocornut and others (to all of them, my most sincere thanks!). They showed me there is indeed another way, and above all, that this way is actually *superior*. This project is my first serious attempt to put some of these ideas into practice, to see where they take me.

With time, I found some of these ideas I actually knew already, I had only be taught very early on to forget about that stuff and write yet another stupid class hierarchy instead. I've seriously rediscovered the joy of programming, and why I started doing this in the first place. And so, just for fun, and also to make sure I don't forget them ever again, I've decided to write down my very own

<br/>

   :: ZEN OF PROGRAMMING ::

 · Complexity is the enemy<br/>
 · Quality is a metric<br/>
 · Beauty matters<br/>
 · Consistency is the mother of mastery<br/>
 · Code is for people, not machines<br/>
 · Your dependencies will enslave you<br/>
 · Your turnaround is way too long<br/>
 · Dumb code is very easy to debug<br/>
 · Marrying data and behaviour is like marrying Heaven and Hell<br/>
 · C++ is just evil<br/>
 · When tempted to abstract away, think twice<br/>
 · When tempted to design for the future, don't<br/>
 · Code will reveal its own design to those who listen<br/>
 · When in doubt, make it an explicit parameter<br/>
 · Implicit-ness is like karma. It will come back and bite you<br/>
 · Don't be afraid of long functions<br/>
 · Focus. Pay attention. The devil is in the details<br/>


