# CSAPP-Lab
Here is the main source code of Linux Shell Lab and Http Proxy Lab, as part of my practice projects in curriculum ICS(CS:APP) in PKU.

shell.c contains code about a simple linux shell, which supports runing jobs in foreground/background, I/O redirection and job management, etc. And writeup-shell.txt is detailed description and requirements need to be fulfilled about the shell. 

proxy.c implements a simple multithread proxy with cache. When the proxy receives a request from browser, it creates a new thread to manipulate it and check whether the URL was cached. If not, the proxy will send the same request to server and get the content, which will be cached and sent back to browser. The cache uses LRU eviction strategy. And writeup-proxy.txt is corresponding documentation about the detailed assignment requirements.

Anyone interested can find more information about labs of CS:APP at http://csapp.cs.cmu.edu/2e/labs.html
