README

For now, testfile.cpp contains the most updated code for the router program. 

Functionality which is yet to be implemented - 

1) Implementing Bellman-Ford on the edges to determine the shortest path.
	Problems - I currently can't figure out an efficient and easy to use data structure
		to store the nodes(node name, port, nextNode) while simultaneously apply
		Bellman-Ford on it.
		**nextNode contains the name of the node to which the packet needs to passed
			for shortest path transmission.

2) Shutting down the node - For now, if the user selects the exit choice on the CLI, that shuts
	down the router. I need a way to shut down the router if ANOTHER router sends a shutdown
	command.
