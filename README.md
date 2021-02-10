# LoadBalancer

This program implements LoadBalancer in client-server mode, where the server acts as the LoadBalancer and the clients act as Providers. The same program works both as client and server.
I have used CLion free source editor to create and execute C++ program that supports C++14 as well. So open the project in CLion, build and run it. This will kick off the LoadBalancer(server).
Once the LoadBalancer is up, start the client from terminal window to bind on port 8888 using:

nc localhost 8888 on MacOS, using netcat which is equivalent to telnet

There can be multiple Providers connected to the LoadBalancer. There are still many limitations to the program, some of them are as mentioned below:

 * 1) Once the LoadBalancer( the server) is up, the clients(Providers) can connect to it. As soon as the LoadBalancer goes down all
 * , all the clients connected to it are also down.
 * 2) Initially when a Provider comes up, it's unique identifier is generated and it is marked as 'Free'
 * 3) If a Provider(client) disconnects, it sends a message to the LoadBalancer(server). There is no means of soft delete
 * at the moment in the program.
 * 4) The call of randomProvider and roundRobinProvider has been driven by a string of 'rand' or 'rRobin' passed from the
 * Provider(client)
 * 5) There is no way of HealthCheck At the moment as the Provider(client) is either up and running or disconnects using ctrl+C
 * , these are only 2 states.( Future enhancement - how to handle this further in this scenario)
 * 6) Once a Provider is allocated it has to be disconnected, there is no provision of freeing( Future enhancement)
