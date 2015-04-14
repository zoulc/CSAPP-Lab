# CSAPP-Lab
Here is the main source code of Linux Shell Lab and Http Proxy Lab, as part of my practice projects in curriculum ICS(CS:APP) in PKU.

shell.c contains code about a simple Unix shell that supports job control and I/O redirection. When the user types in a new command line, the shell parses it and initializes a new process to run the job. If the user types ctrl-c or ctrl-z, the shell will send the corresponding signal to notify the child process to stop and manage their information in job list.

proxy.c implements a simple multithread proxy with cache. When the proxy receives a request from browser, it creates a new thread to manipulate it and check whether the URL was cached. If not, the proxy will send the same request to server and get the content, which will be cached and sent back to browser. The cache uses LRU eviction strategy. And writeup-proxy.txt is corresponding documentation about the detailed assignment requirements.

Anyone interested can find more information about labs of CS:APP at http://csapp.cs.cmu.edu/2e/labs.html
