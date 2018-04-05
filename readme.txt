README

"testfile.cpp" contains the most updated code for the program. 
The format to run the router is "./router <ROUTER_NAME> <PORT>"
** The router name can be anything between A to F (if not, the program will not work as expected)
** Router port number has to be mapped accordingly, example 10000 for A, 10001 for B, ....., 10005 for F.
	Any other port number will lead to unexpected results. 

***So basically, router names can be only between A-F and can only have ports 10000-10005 depending on the router's name.***

Functionality not yet implemented - 

1) Router Disappearance - Currently, the router can't figure out if a router has disappeared from the network

2) Router Shutdown - Shutdown works but not completely, i.e the router itself shuts down but the other router'shuts
	don't find out about it's exit from the network.
