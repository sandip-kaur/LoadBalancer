/* Implement LoadBalancer functionality with a Server and multiple clients ion of freeing( Future enhancement)
 * */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <iostream>
#include <map>

#define TRUE 1
#define FALSE 0
#define PORT 8888
#define LEN 12
#define MAX_PROVIDERS 10

// 2nd element of Map to store Provider unique number and status whether Free or Busy
struct provDetails{
    std::string sProvider;
    std::string sStatus;
};

//Map to store the Provider details and this is used between functions, so kept global for now.
std::map<int, provDetails> pMap;
std::map<int, provDetails>::iterator it;

/* Class - Provider
 * Generates a unique string and contains functions to include or exclude a provider
 */
class Provider
{
    constexpr static auto alphanum  =
            "0123456789"
            "!@#$%^&*"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
public:
    std::string getProvider();
    int includeProvider(const std::string strInclPro, int sd);
    int excludeProvider(int sd);
};

/* getProvider() - function to generate a unique random string */
std::string Provider::getProvider()
{
    std::string tmp_s;
    srand( (unsigned) time(NULL) * getpid());
    tmp_s.reserve(LEN);
    for (int i = 0; i < LEN; ++i)
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    return tmp_s;
}

/* includeProvider() - includes a Provider
 * @strInclPro - input parameter containing the unique generated string, to be included into the map structure
 * @sd - input the unique socket id generated when the Provider connects to the LoadBalancer
 * */
int Provider::includeProvider(const std::string strInclPro, int sd)
{
    //populate relevant provider data into the map and set the status to be 'Free' to begin with
    provDetails pDet = { strInclPro, "Free"};
    if (pMap.size() < MAX_PROVIDERS) {
        pMap.insert(std::pair<int, provDetails> (sd, pDet));
    }
    else {
        std::cerr << "Maximum number of Providers Reached" << std::endl;
    }
    return 0;
}

/* excludeProvider() - excludes a Provider
 * @sd - pass a key to fetch the correct data from the Map
 * */
int Provider::excludeProvider(int sd)
{
    std::map<int, provDetails>::iterator it;
    if ((it = pMap.find(sd)) != pMap.end())
    {
        pMap.erase(it);
    }
    else
    {
        std::cerr << "Provider does not exist in the list" << std::endl;
    }
    return 0;
}

/* Class - LoadBalancer
 * getLoadBalancer - returns a random or sequential Provider from the Provider List
 * Trim functions - used to trim passed string for escape characters
 */
class loadBalancer
{
    static int rndRobPos;
public:
    int getLoadBalancer(int sd, std::string buff);

    std::string& ltrim(std::string& str, const std::string& chars = "\t\n\v\f\r ")
    {
        str.erase(0, str.find_first_not_of(chars));
        return str;
    }

    std::string& rtrim(std::string& str, const std::string& chars = "\t\n\v\f\r ")
    {
        str.erase(str.find_last_not_of(chars) + 1);
        return str;
    }

    std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ")
    {
        return ltrim(rtrim(str, chars), chars);
    }

};

int loadBalancer::rndRobPos = 0;

/*
 * Function to find a random Provider or a round Robin invocation of a Provider
 */
int loadBalancer::getLoadBalancer(int sd, std::string buff) {
    int i = 0;
    int sSelectRand;
    srand( (unsigned) time(NULL) * getpid());
    bool found = false;
    // Check whether to pick up provider randomly or on round robin manner
    if (trim(buff)== "rand")
    {
        long rndNum = rand() % pMap.size();
        while(TRUE) {
            if ((it = pMap.find(rndNum)) != pMap.end()) {
                if (it->second.sStatus == "Free") {
                    sSelectRand = it->first;
                    it->second.sStatus = "Busy";
                    break;
                } else {
                    ++rndNum;
                }
            }
            else
            {
                ++rndNum;
                if (rndNum >= MAX_PROVIDERS)
                {
                    rndNum = 0;
                }
            }
        }
    }
    else if (trim(buff) == "rRobin")
    {
        if (rndRobPos >= MAX_PROVIDERS)
        {
            rndRobPos = 0;
        }

        while(TRUE) {
            if ((it = pMap.find(rndRobPos)) != pMap.end()) {
                if (it->second.sStatus == "Free") {
                    sSelectRand = it->first;
                    it->second.sStatus = "Busy";
                    break;
                }
                else {
                    ++rndRobPos;
                }
            }
            else {
                ++rndRobPos;
            }
        }
    }
    return sSelectRand;
}

int main(int argc , char *argv[])
{
    int opt = TRUE;
    int master_socket , addrlen , new_socket , client_socket[30] ,
            max_clients = 30 , activity, i , valread , sd;
    int max_sd;
    struct sockaddr_in address;

    char buffer[1025]; //data buffer of 1K

    //set of socket descriptors
    fd_set readfds;
    Provider pObj;
    loadBalancer lbObj;

    //a message
    char *message;

    //initialise all client_socket[] to 0 so not checked
    for (i = 0; i < max_clients; i++)
    {
        client_socket[i] = 0;
    }

    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)
    {
        std::cerr << "socket failed";
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections ,
    //this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
                   sizeof(opt)) < 0 )
    {
        std::cerr << "setsockopt";
        exit(EXIT_FAILURE);
    }

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)
    {
        std::cerr << "bind failed";
        exit(EXIT_FAILURE);
    } else {
        printf("Listener on port %d \n", PORT);

        //try to specify maximum of 10 pending connections for the master socket
        if (listen(master_socket, 10) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        //accept the incoming connection
        addrlen = sizeof(address);
        puts("Waiting for connections ...");

        while (TRUE) {
            //clear the socket set
            FD_ZERO(&readfds);

            //add master socket to set
            FD_SET(master_socket, &readfds);
            max_sd = master_socket;

            //add child sockets to set
            for (i = 0; i < max_clients; i++) {
                //socket descriptor
                sd = client_socket[i];

                //if valid socket descriptor then add to read list
                if (sd > 0)
                    FD_SET(sd, &readfds);

                //highest file descriptor number, need it for the select function
                if (sd > max_sd)
                    max_sd = sd;
            }

            //wait for an activity on one of the sockets , timeout is NULL ,
            //so wait indefinitely
            activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

            if ((activity < 0) && (errno != EINTR)) {
                printf("select error");
            }

            //If something happened on the master socket ,
            //then its an incoming connection
            if (FD_ISSET(master_socket, &readfds)) {
                if ((new_socket = accept(master_socket,
                                         (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                perror("accept");

                //inform user of socket number - used in send and receive commands
                printf("New connection , socket fd is %d , ip is : %s , port : %d\n", new_socket,
                       inet_ntoa(address.sin_addr), ntohs
                       (address.sin_port));
                //getLoadBalancer.get( - generates te Provider string - pass to client
                //send new connection greeting message
                std::string sndStr = pObj.getProvider();
                message = &sndStr[0];
                if (send(new_socket, message, strlen(message), 0) != strlen(message)) {
                    perror("send");
                }

                puts("Welcome message sent successfully");

                //add new socket to array of sockets
                for (i = 0; i < max_clients; i++) {
                    //if position is empty
                    if (client_socket[i] == 0) {
                        client_socket[i] = new_socket;
                        std::cout << "Adding to list of sockets as:" << i << " New Socket:" << client_socket[i]  << std::endl;
                        pObj.includeProvider(message, new_socket);
                        auto it = pMap.find(new_socket);
                        if ( it != pMap.end()){
                            std::cout << it->first << " " << it->second.sProvider << " " << it->second.sStatus << std::endl;
                        }

                        break;
                    }
                }
            }

            //else its some IO operation on some other socket
            for (i = 0; i < max_clients; i++) {
                sd = client_socket[i];
                if (FD_ISSET(sd, &readfds)) {
                    //Check if it was for closing , and also read the
                    //incoming message
                    if ((valread = read(sd, buffer, 1024)) == 0) {
                        //Somebody disconnected , get his details and print
                        getpeername(sd, (struct sockaddr *) &address, \
                        (socklen_t *) &addrlen);
                        printf("Host disconnected , ip %s , port %d \n",
                               inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                        //Close the socket and mark as 0 in list for reuse
                        std::cout << "sd :" << sd << std::endl;
                        pObj.excludeProvider(sd);
                        close(sd);
                        client_socket[i] = 0;
                        auto it = pMap.find(sd);
                        if ( it != pMap.end()){
                            std::cout << it->first << " " << it->second.sProvider << " " << it->second.sStatus << std::endl;
                        }
                    }
                    else {
                        //terminate the string with NULL
                        buffer[valread] = '\0';
                        std::string sTmpBuf = std::string(buffer);

                        if ((lbObj.trim(sTmpBuf) == "rand") || (lbObj.trim(sTmpBuf) == "rRobin"))
                        {
                            int val = lbObj.getLoadBalancer(sd, lbObj.trim(sTmpBuf));
                            auto it = pMap.find(val);
                            if ( it != pMap.end()){
                                std::cout << it->first << " " << it->second.sProvider << " " << it->second.sStatus << std::endl;
                            }
                        }
                        else
                        {
                            send(sd, buffer, strlen(buffer), 0);
                        }
                    }
                }
            }
        }
        return 0;
    }
}





