//
//  server.c
//  Client
//
//  Created by Kaley Chicoine on 10/28/17.
//  Copyright © 2017 Kaley Chicoine. All rights reserved.
//

#include "server.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>
#include <stdint.h>
#include "duckchat.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <errno.h>
#include <time.h>


int validUser(struct sockaddr_in* test, int numUsers) {
    for (int i = 0; i < numUsers; i++) {
        if (test->sin_addr.s_addr == users[i].useraddr.sin_addr.s_addr &&
            test->sin_port == users[i].useraddr.sin_port)
            return i;
    }
    return -1;
}
int validChannel(char *channelName, int countChannels) {
    for (int i = 0; i < countChannels; i++) {
        if (strcmp(channelName, channels[i].channelname) == 0)
            return i;
    }
    return countChannels;
}

void removeChannelFromUser(struct aUser* user, int remIndex, int totalChannels) {
    for (int i = remIndex; i < totalChannels-1; i++) {
        memcpy(&user->userChannels[i],&user->userChannels[i+1], USERNAME_MAX);
    }
    memset(&user->userChannels[user->nChannels], 0, USERNAME_MAX);
    user->nChannels --;
}
void removeUserFromChannel(struct aChannel* channel, int remIndex, int totalUsers) {
    for (int i = remIndex; i < totalUsers-1; i++) {
        memcpy(&channel->channelUsers[i],&channel->channelUsers[i+1], sizeof(aUser));
    }
    memset(&channel->channelUsers[channel->nUsers], 0, sizeof(aUser));
    channel->nUsers --;
}

//args: remove channel from user, remove user from channel
int leave(struct aUser* user, struct aChannel* channel) {
    int i,j;
    //remove channel from user's list
    for (i = 0; i < user->nChannels; i++) {
        if (strcmp(user->userChannels[i], channel->channelname) == 0) {
            removeChannelFromUser(user, i, user->nChannels);
            i = user->nChannels + 1;
        }
    }
    if (i == user->nChannels) {
        printf("Could not find channel in user\n");
        return -1;
    }
    //remove user from channel's list
    for (j = 0; j < channel->nUsers; j++) {
        if (user->useraddr.sin_addr.s_addr == channel->channelUsers[j].useraddr.sin_addr.s_addr &&
            user->useraddr.sin_port == channel->channelUsers[j].useraddr.sin_port) {
            removeUserFromChannel(channel, j, channel->nUsers);
            j = channel->nUsers + 1;
        }
    }
    if (j == channel->nUsers) {
        printf("Could not find user in channel\n");
        return -2;
    }
    return 0;
}

int removeDuplicateChannels(int numChannels) {
    for (int i = 0; i < numChannels; i++) {
        if (channels[i].nUsers == 0 && channels[i].nNeighbors == 0) {
            printf("Removing empty channel %s\n", channels[i].channelname);
            for (int j = i; j < numChannels - 1; j++) {
                memcpy(&channels[j], &channels[j+1], sizeof(aChannel));
            }
            memset(&channels[numChannels], 0, sizeof(aChannel));
            i --;
            numChannels --;
        }
    }
    return numChannels;
}
int removeUser(int numUsers, int user) {
    for (int i = user; i < numUsers-1; i++) {
        memcpy(&users[i], &users[i+1], sizeof(aUser));
    }
    memset(&users[numUsers], 0, sizeof(aUser));
    numUsers --;
    return numUsers;
}
bool checkUserInChannel(struct aUser* user, struct aChannel* channel) {
    for (int i = 0; i < channel->nUsers; i++) {
        if (user->useraddr.sin_addr.s_addr == channel->channelUsers[i].useraddr.sin_addr.s_addr &&
            user->useraddr.sin_port == channel->channelUsers[i].useraddr.sin_port)
            return true;
    }
    return false;
}
void serverError(struct text_error* txtError, char* message) {
    txtError->txt_type = TXT_ERROR;
    strcpy(txtError->txt_error, message);
}
void say(struct text_say* txtSay, char* message, char* user, char* channel) {
    txtSay->txt_type = TXT_SAY;
    strcpy(txtSay->txt_username, user);
    strcpy(txtSay->txt_channel, channel);
    strcpy(txtSay->txt_text, message);
}
void list(struct text_list* txtList, int numChannels){
    txtList->txt_type = TXT_LIST;
    txtList->txt_nchannels = numChannels;
    memset(txtList->txt_channels, 0, sizeof(channel_info) * numChannels);
    for (int i = 0; i < numChannels; i++) {
        strncpy(txtList->txt_channels[i].ch_channel, channels[i].channelname, sizeof(channel_info));
    }
}
void who(struct text_who* txtWho, int numUsers, int channel) {
    txtWho->txt_type = TXT_WHO;
    txtWho->txt_nusernames = numUsers;
    strcpy(txtWho->txt_channel, channels[channel].channelname);
    memset(txtWho->txt_users, 0, sizeof(user_info) * numUsers);
    for (int i = 0; i < numUsers; i++) {
        strncpy(txtWho->txt_users[i].us_username, channels[channel].channelUsers[i].username, sizeof(user_info));
    }
}
void removeUserFromAllChannels(int user) {
    for (int i = 0; i < numChannels; i++) {
        for (int j = 0; j < channels[i].nUsers; j++) {
            if (users[user].useraddr.sin_addr.s_addr == channels[i].channelUsers[j].useraddr.sin_addr.s_addr &&
                users[user].useraddr.sin_port == channels[i].channelUsers[j].useraddr.sin_port) {
                removeUserFromChannel(&channels[i], j, channels[i].nUsers);
                j = channels[i].nUsers;
            }
        }
    }
    numUsers = removeUser(numUsers, user);
    numChannels = removeDuplicateChannels(numChannels);
}
void setTwoAlarmUser() {
   // struct itimerval newItVal;
    if (signal(SIGALRM, onTwoAlarmUser) == SIG_ERR) {
        printf("Could not catch alarm\n");
    }
    //set new timer
    newItVal2User.it_value.tv_sec = TIMEOUT2;
    newItVal2User.it_value.tv_usec = 0;
    newItVal2User.it_interval = newItVal2User.it_value;

    if (setitimer(ITIMER_REAL, &newItVal2User, NULL) == -1) {
        printf( "error calling setitimer()\n");
    }
    
}
static void onTwoAlarmUser(UNUSED int sig) {
    struct timeval currentTime;
    gettimeofday(&currentTime,NULL);
    for (int i = 0; i < numUsers; i++) {
        if ((currentTime.tv_sec - users[i].lastData.tv_sec) > TIMEOUT2) {
            printf("Forcibly removing %s\n", users[i].username);
            removeUserFromAllChannels(i);
        }
    }
}
static void printDebug(struct sockaddr_in userServ) {
    char str[INET_ADDRSTRLEN], str2[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(serveraddr.sin_addr),str, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(userServ.sin_addr), str2, INET_ADDRSTRLEN);
    printf("%s:%i %s:%i ", str, serverPort, str2, ntohs(userServ.sin_port));
}

void s2sJoin(struct request_s2s_join* join, char* channel) {
    join->req_type = REQ_S2S_JOIN;
    strcpy(join->req_channel, channel);
}
void s2sSay(struct request_s2s_say* say, char* message, char* username, char* channel, long long randval=0) {
    if (randval == 0) {
        FILE *f;
        f = fopen("/dev/random", "r");
        fread(&randval, sizeof(randval), 1, f);
        fclose(f);
    }
    srand((unsigned) randval);
    say->req_id = rand();
    
    say->req_type = REQ_S2S_SAY;
    strcpy(say->req_channel, channel);
    strcpy(say->req_text, message);
    strcpy(say->req_username, username);
}
void s2sLeave(struct request_s2s_leave* leave, char* channel) {
    leave->req_type = REQ_S2S_LEAVE;
    strcpy(leave->req_channel, channel);
}
void removeServerFromChannel(struct sockaddr_in* remove, int channel) {
    int loc = channels[channel].nNeighbors;
    for (int i = 0; i < channels[channel].nNeighbors; i++) {
        if (remove->sin_addr.s_addr == channel[channels].channelNeighbors[i].sin_addr.s_addr &&
            remove->sin_port == channel[channels].channelNeighbors[i].sin_port) {
            loc = i;
        }
    }
    if (loc < channels[channel].nNeighbors) {
        for (int i = loc; i < channels[channel].nNeighbors-1; i++ ) {
            memcpy(&channels[channel].channelNeighbors[i], &channels[channel].channelNeighbors[i+1],sizeof(struct sockaddr_in));
        }
        memset(&channels[channel].channelNeighbors[channels[channel].nNeighbors-1], 0, sizeof(aChannel));
        channels[channel].nNeighbors --;
    }
    else {
        printf("server %i not in channel %s\n", ntohs(remove->sin_port), channels[channel].channelname);
    }
}
static void addTag(long long tag) {
    messageTags[numMessages] = tag;
    numMessages ++;
}
bool checkTags(long long tag) {
    for (int i = 0; i < numMessages; i++) {
        if (tag == messageTags[i]) {
            return true;
        }
    }
    return false;
}

/*static void testPrint(struct sockaddr_in serveraddr) {
    for (int i = 0; i < numChannels; i++) {
        printf("server: %i, channel: %s\n", ntohs(serveraddr.sin_port), channels[i].channelname);
        for (int j = 0; j < channels[i].nNeighbors; j++) {
            printf("   neighbor: %i\n", ntohs(channels[i].channelNeighbors[j].sin_port));
        }
    }
}*/

/*static void tagPrint(struct sockaddr_in serveraddr) {
    printf("%i messages: \n", ntohs(serveraddr.sin_port));
    for (int i = 0; i < numMessages; i++) {
        printf("   %i\n", messageTags[i]);
    }
}*/
void setOneAlarm() {
    static struct sigaction sa;
    static struct sigevent sevp;
    //newItVal1 = its
    oldItVal1 = newItVal1;
    
    memset (&sevp, 0, sizeof (struct sigevent));
    sevp.sigev_value.sival_ptr = &timerid1;
    sevp.sigev_notify = SIGEV_SIGNAL;
    sevp.sigev_notify_attributes = NULL;
    sevp.sigev_signo = SIGUSR1;
    //sevp.sigev_notify_function=onOneAlarm;
    
    newItVal1.it_interval.tv_sec = 0;
    newItVal1.it_interval.tv_nsec = 0;
    
    newItVal1.it_value.tv_sec = TIMEOUT1;
    newItVal1.it_value.tv_nsec = 0;
    
    sa.sa_handler = onOneAlarm;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    
    if (timer_create(CLOCK_REALTIME, &sevp, &timerid1) == -1) {
        printf("failed to create timer");
        return;
    }
    if (timer_settime(timerid1, 0, &newItVal1, NULL) == -1) {
        printf("failed to set timer");
        return;
    }
}
static void onOneAlarm(UNUSED int sig) {
    struct request_s2s_join reqS2sJoin;
    for (int i = 0; i < numChannels; i++) {
        s2sJoin(&reqS2sJoin, channels[i].channelname);
        for (int j = 0; j < numNeighbors; j++) {
            if (sendto(sockfd, &reqS2sJoin, sizeof(reqS2sJoin), 0, (struct sockaddr *)&neighbors[j], sizeof(struct sockaddr)) < 0) {
                printf("error in join send\n");
                perror("lol error: ");
            }
            printDebug(neighbors[j].neighboraddr);
            printf("send s2s Soft Join %s\n", reqS2sJoin.req_channel);
        }

    }
    setOneAlarm();
}
void setTwoAlarmServer() {
    static struct sigaction sa;
    static struct sigevent sevp;
    //newItVal1 = its
    oldItVal2Server = newItVal2Server;
    
    memset (&sevp, 0, sizeof (struct sigevent));
    sevp.sigev_value.sival_ptr = &timerid2;
    sevp.sigev_notify = SIGEV_SIGNAL;
    sevp.sigev_notify_attributes = NULL;
    sevp.sigev_signo = SIGUSR1;
    //sevp.sigev_notify_function=onOneAlarm;
    
    newItVal2Server.it_interval.tv_sec = 0;
    newItVal2Server.it_interval.tv_nsec = 0;
    
    newItVal2Server.it_value.tv_sec = TIMEOUT2;
    newItVal2Server.it_value.tv_nsec = 0;
    
    sa.sa_handler = onTwoAlarmServer;
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);
    
    if (timer_create(CLOCK_REALTIME, &sevp, &timerid2) == -1) {
        printf("failed to create timer");
        return;
    }
    if (timer_settime(timerid2, 0, &newItVal2Server, NULL) == -1) {
        printf("failed to set timer");
        return;
    }
}
static void onTwoAlarmServer(UNUSED int sig) {
    struct timeval currentTime;
    gettimeofday(&currentTime,NULL);
    for (int i = 0; i < numNeighbors; i++) {
        if ((currentTime.tv_sec - neighbors[i].lastData.tv_sec) > TIMEOUT2) {
            printf("Forcibly removing %i\n", ntohs(neighbors[i].neighboraddr.sin_port));
            for (int j = 0; j < numChannels; j++) {
                removeServerFromChannel(&neighbors[i].neighboraddr, j);
            }
            numNeighbors --;
        }
    }
}
int main(int argc, char *argv[] ) {
    char hostname[UNIX_PATH_MAX];
    struct sockaddr_in clientaddr;
    struct request_login reqLogin;
    struct request_logout reqLogout;
    struct request_join reqJoin;
    struct request_leave reqLeave;
    struct request_list reqList;
    struct request_say reqSay;
    struct request_who reqWho;
    struct request_keep_alive reqKeepAlive;
    struct request_s2s_join reqS2sJoin;
    struct request_s2s_say reqS2sSay;
    struct request_s2s_say reqS2sSay2;
    struct request_s2s_leave reqS2sLeave;
    struct text_error txtError;
    struct text_say txtSay;
    struct text_list txtList;
    struct text_who txtWho;
    struct request req;
    socklen_t len, peekLen;
    int n;
    struct hostent *hp;
    char *hostaddrp;
    fd_set readset;

    //alarm to send soft join
    setOneAlarm();
    //alarm to check for joins
    setTwoAlarmUser();
    setTwoAlarmServer();
    //check inputs
    
    if (argc % 2 == 0) {
        printf("Must enter address and port for each server.\n");
        return 1;
    }
    
    for (int i = 3; i < argc; i += 2) {
        if (strlen(argv[i]) > UNIX_PATH_MAX) {
            printf("Path too long: %i characters.\n", int(strlen(argv[1])));
            return 1;
        }
        else {
            strcpy(hostname, argv[i]);
        }
        hp = gethostbyname(hostname);
        if (!hp) {
            printf("HOST ERROR: Could not obtain address");
            return 1;
        }
        memset(&neighbors[numNeighbors], 0, sizeof(sockaddr_in));
        neighbors[numNeighbors].neighboraddr.sin_family = AF_INET;
        neighbors[numNeighbors].neighboraddr.sin_port = htons(strtol(argv[i+1], NULL, 10));
        if (gethostname(hostname, sizeof(hostname)) < 0) {
            printf("Error getting local host\n");
            return 1;
        }
        memcpy(&(neighbors[numNeighbors].neighboraddr.sin_addr), hp->h_addr_list[0], sizeof(struct in_addr));
        inet_pton(AF_INET, hp->h_addr_list[0], &neighbors[numNeighbors].neighboraddr.sin_addr);
        numNeighbors ++;
    }
    //set up socket
    if (strlen(argv[1]) > UNIX_PATH_MAX) {
        printf("Path too long: %i characters.\n", int(strlen(argv[1])));
        return 1;
    }
    else {
        strcpy(hostname, argv[1]);
    }
    serverPort = strtol(argv[2], NULL, 10);
    
    hp = gethostbyname(hostname);
    if (!hp) {
        printf("HOST ERROR: Could not obtain address");
        return 1;
    }
    memset((char *)&serveraddr, 0, sizeof(sockaddr_in));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(strtol(argv[2], NULL, 10));
    if (gethostname(hostname, sizeof(hostname)) < 0) {
        printf("Error getting local host\n");
        return 1;
    }
    memcpy(&serveraddr.sin_addr, hp->h_addr_list[0], sizeof(struct in_addr));
    inet_pton(AF_INET, hp->h_addr_list[0], &(serveraddr.sin_addr));
    //set socket info
    
    //bind to socket
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        printf("SOCKET ERROR\n");
        return 1;
    }
    n = bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if(n < 0) {
        printf("BIND ERROR\n");
        return 1;
    }
    
    
    
    len = sizeof(clientaddr);
    peekLen = sizeof(req);
    while(true) {
        FD_ZERO(&readset);
        FD_SET(sockfd, &readset);
        if (select(sockfd + 1, &readset, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                //timers firing
                continue;
            } else {
                printf("Select error.\n");
            }
        }
        else {
            memset((char *) &clientaddr, 0, sizeof(clientaddr));
            if (recvfrom(sockfd, &req, sizeof(req), MSG_PEEK, (struct sockaddr *) &clientaddr, &peekLen) < 0) {
                printf("PEEK RECV ERROR\n");
                
                serverError(&txtError, (char *)"Problem receiving message. Please resend.\n");
                if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                    printf("SEND ERROR");
                }
            } else {
                switch (req.req_type) {
                        
                    case REQ_LOGIN :
                        if (numUsers < USERS_MAX) {

                            if (recvfrom(sockfd, &reqLogin, sizeof(reqLogin), 0, (struct sockaddr *) &users[numUsers].useraddr, &len) < 0) {
                                printf("LOGIN ERROR\n");
                                serverError(&txtError, (char *)"Login error. Please try again.\n");
                                if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                    printf("SEND ERROR\n");
                                }
                            } else {

                                //add user to list of users
                                strcpy(users[numUsers].username, reqLogin.req_username);
                                users[numUsers].username[USERNAME_MAX] = '\0';
                                hp = gethostbyaddr((const char *)&users[numUsers].useraddr.sin_addr.s_addr,
                                                   sizeof(users[numUsers].useraddr.sin_addr.s_addr), AF_INET);
                                if (!hp) {
                                    printf("GET HOST ERROR\n");
                                }
                               hostaddrp = inet_ntoa(users[numUsers].useraddr.sin_addr);
                                if (hostaddrp == NULL) {
                                    printf("ERROR on inet_ntoa\n");
                                }
                                gettimeofday(&users[numUsers].lastData,NULL);
                                printDebug(users[numUsers].useraddr);
                                printf("recv Request Login\n");
                                numUsers ++;
                            }
                        } else {
                            printf("Too many users.\n");
                            serverError(&txtError, (char *)"Server full: too many users. Please try again later.\n");
                            if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                printf("SEND ERROR\n");
                            }
                        }
                        break;
                        
                    case REQ_JOIN :
                        if (recvfrom(sockfd, &reqJoin, sizeof(reqJoin), 0, (struct sockaddr *) &clientaddr, &len) < 0) {
                            printf( "JOIN ERROR\n");
                            serverError(&txtError, (char *)"Problem receiving join message. Please try again.\n");
                            if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                printf("SEND ERROR\n");
                            }
                        } else {
                            int user = validUser(&clientaddr, numUsers);
                            int channel = validChannel(reqJoin.req_channel, numChannels);
                            //valid user
                            if (user >= 0) {
                                gettimeofday(&users[user].lastData,NULL);
                                printDebug(users[user].useraddr);
                                printf("recv Request Join %s\n", reqJoin.req_channel);
                                //server is already in channel
                                if (channel < numChannels) {
                                    if (numChannels < NCHANNEL_MAX && channel >= 0 && !checkUserInChannel(&users[user], &channels[channel])) {
                                        //add channel to user
                                        int lastChannel = users[user].nChannels;
                                        strncpy(users[user].userChannels[lastChannel], reqJoin.req_channel, strlen(reqJoin.req_channel));
                                        users[user].userChannels[lastChannel][CHANNEL_MAX] = '\0';
                                        users[user].nChannels ++;
                                        //add user to channel
                                        channels[channel].channelUsers[channels[channel].nUsers] = users[user];
                                        channels[channel].nUsers ++;
                                        
                                    }
                                    //user tries to join channel they are already in
                                    else if (channel >= 0 && checkUserInChannel(&users[user], &channels[channel])) {
                                        printf("%s attempts to join %s which they have already joined\n",users[user].username, channels[channel].channelname);
                                    }
                                
                                    //max channels reached
                                    else {
                                        printf("%s attempting to join channel- max channels reached.\n", users[user].username);
                                        serverError(&txtError, (char *)"Server full: too many channels. Please use existing channels.\n");
                                        if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&users[user].useraddr, sizeof(users[user].useraddr)) < 0) {
                                            printf( "SEND ERROR\n");
                                        }
                                    }
                                }
                                //server not in channel
                                else {
                                    //add channel
                                    int lastChannel = users[user].nChannels;
                                    strncpy(users[user].userChannels[lastChannel], reqJoin.req_channel, strlen(reqJoin.req_channel));
                                    users[user].userChannels[lastChannel][CHANNEL_MAX] = '\0';
                                    users[user].nChannels ++;
                                    //add user to channel
                                    channels[channel].channelUsers[channels[channel].nUsers] = users[user];
                                    channels[channel].nUsers ++;
                                    strcpy(channels[channel].channelname, reqJoin.req_channel);
                                    channels[channel].channelname[CHANNEL_MAX] = '\0';
                                    numChannels ++;
                                    channels[channel].nNeighbors = 0;
                                    //add neighbors to channel
                                    for (int i = 0; i < numNeighbors; i++) {
                                        memcpy(&channels[channel].channelNeighbors[i], &neighbors[i], sizeof(struct sockaddr_in));
                                        channels[channel].nNeighbors += 1;

                                        //send join to neighbors
                                        s2sJoin(&reqS2sJoin, reqJoin.req_channel);
                                        if (sendto(sockfd, &reqS2sJoin, sizeof(reqS2sJoin), 0, (struct sockaddr *)&neighbors[i], sizeof(struct sockaddr)) < 0) {
                                            printf("error in join send\n");
                                            perror("lol error: ");
                                        }
                                        printDebug(neighbors[i].neighboraddr);
                                        printf("send s2s Join %s\n", reqJoin.req_channel);
                                    }
                                }
                            }
                        }
                        //testPrint(serveraddr);
                        break;
                        
                    case REQ_S2S_JOIN :
                        if (recvfrom(sockfd, &reqS2sJoin, sizeof(reqS2sJoin), 0, (struct sockaddr *) &clientaddr, &len) < 0) {
                            printf( "JOIN ERROR\n");
                            serverError(&txtError, (char *)"Problem receiving s2s join message. Please try again.\n");
                            if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                printf("SEND ERROR\n");
                            }
                        } else {
                            printDebug(clientaddr);
                            printf("recv S2S Join %s\n", reqS2sJoin.req_channel);
                            for (int i = 0; i < numNeighbors; i++) {
                                //find which neighbor sent join
                                if (neighbors[i].neighboraddr.sin_addr.s_addr == clientaddr.sin_addr.s_addr &&
                                    neighbors[i].neighboraddr.sin_port == clientaddr.sin_port) {
                                    gettimeofday(&neighbors[i].lastData, NULL);
                                }
                            }
                            
                            int channel = validChannel(reqS2sJoin.req_channel, numChannels);
                            //if not in channel, join and forward
                            if (channel == numChannels) {
                                channels[channel].nUsers = 0;
                                channels[channel].nNeighbors = 0;
                                strcpy(channels[channel].channelname, reqS2sJoin.req_channel);
                                channels[channel].channelname[CHANNEL_MAX] = '\0';
                                numChannels ++;
                                for (int i = 0; i < numNeighbors; i++) {
                                    //add neighbors to channel
                                    memcpy(&channels[channel].channelNeighbors[i], &neighbors[i], sizeof(struct sockaddr_in));
                                    channels[channel].nNeighbors ++;

                                    //if not server sending message, forward message
                                    if (neighbors[i].neighboraddr.sin_addr.s_addr != clientaddr.sin_addr.s_addr ||
                                        neighbors[i].neighboraddr.sin_port != clientaddr.sin_port) {
                                        //send join to neighbors
                                        s2sJoin(&reqS2sJoin, channels[channel].channelname);
                                        if (sendto(sockfd, &reqS2sJoin, sizeof(reqS2sJoin), 0, (struct sockaddr *)&neighbors[i], sizeof(struct sockaddr)) < 0) {
                                            printf("%i broke in reqs2s join", ntohs(neighbors[i].neighboraddr.sin_port));
                                            perror("lol error: ");
                                        }
                                        printDebug(neighbors[i].neighboraddr);
                                        printf("send s2s Join %s\n", reqJoin.req_channel);
                                    }
                                }
                            }
                            //if in channel, just add sender to channel
                            else {
                                bool test = true;
                                //check if sender is already in channel
                                for (int i = 0; i < numNeighbors; i++) {
                                    if (channels[channel].channelNeighbors[i].sin_addr.s_addr == clientaddr.sin_addr.s_addr &&
                                        channels[channel].channelNeighbors[i].sin_port == clientaddr.sin_port ) {
                                        test = false;
                                    }
                                }
                                if (test) {
                                    memcpy(&channels[channel].channelNeighbors[channels[channel].nNeighbors], &clientaddr, sizeof(struct sockaddr_in));
                                    channels[channel].nNeighbors ++;
                                } else {
                                    printf("sender %i already in channel in %i\n", ntohs(clientaddr.sin_port), ntohs(serveraddr.sin_port));
                                }
                            }
                        }
                        //testPrint(serveraddr);
                            break;
                        
                    case REQ_LOGOUT :
                        if (recvfrom(sockfd, &reqLogout, sizeof(reqLogout), 0, (struct sockaddr *) &clientaddr, &len) < 0) {
                            printf( "PEEK RECV ERROR\n");
                            serverError(&txtError, (char *)"Trouble logging off. Please try again.\n");
                            if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                printf("SEND ERROR\n");
                            }
                        } else {

                            int user = validUser(&clientaddr, numUsers);
                            if (user >= 0) {
                                printDebug(users[user].useraddr);
                                printf("recv Request Logout\n");
                                removeUserFromAllChannels(user);
                            }
                        }
                        break;
                        
                    case REQ_LEAVE :
                        if (recvfrom(sockfd, &reqLeave, sizeof(reqLeave), 0, (struct sockaddr *) &clientaddr, &len) < 0) {
                            printf( "LEAVE ERROR\n");
                            serverError(&txtError, (char *)"Problem leaving channel. Please try again.\n");
                            if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                printf("SEND ERROR\n");
                            }
                        } else {

                            int user = validUser(&clientaddr, numUsers);
                            int channel = validChannel(reqLeave.req_channel, numChannels);
                            
                            //check for valid user
                            if (user >= 0) {
                                printDebug(users[user].useraddr);
                                printf("recv Request Leave %s\n", reqLeave.req_channel);
                                gettimeofday(&users[user].lastData,NULL);
                                //check for valid channel
                                if (channel >= 0) {
                                    leave(&users[user], &channels[channel]);
                                }
                                else {
                                    printf("%s attempting to leave nonexistant channel %s\n", users[user].username, reqLeave.req_channel);
                                    serverError(&txtError, (char *)"Channel does not exist.\n");
                                    if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&users[user].useraddr, sizeof(users[user].useraddr)) < 0) {
                                        printf( "SEND ERROR\n");
                                    }
                                }
                                numChannels = removeDuplicateChannels(numChannels);
                            }
                        }
                        break;
                        
                    case REQ_S2S_LEAVE:
                        if (recvfrom(sockfd, &reqS2sLeave, sizeof(reqS2sLeave), 0, (struct sockaddr *) &clientaddr, &len) < 0) {
                            printf( "LEAVE ERROR\n");
                            serverError(&txtError, (char *)"Problem leaving channel. Please try again.\n");
                            if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                printf("SEND ERROR\n");
                            }
                        } else {
                            printDebug(clientaddr);
                            printf("recv s2s Leave %s\n", reqS2sLeave.req_channel);
                            int channel = validChannel(reqS2sLeave.req_channel, numChannels);
                            if (channel < numChannels) {
                                removeServerFromChannel(&clientaddr, channel);
                                
                            } else {
                                printf("Server tries to leave nonexistant channel\n");
                            }
                        }
                        break;
                    case REQ_SAY :
                        if (recvfrom(sockfd, &reqSay, sizeof(reqSay), 0, (struct sockaddr *) &clientaddr, &len) < 0) {
                            printf("SAY RECV ERROR\n");
                            serverError(&txtError, (char *)"Problem receiving message. Please resend.\n");
                            if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                printf("SEND ERROR\n");
                            }
                        } else {
                            int user = validUser(&clientaddr, numUsers);
                            int channel = validChannel(reqSay.req_channel, numChannels);
                            bool inChannel = checkUserInChannel(&users[user], &channels[channel]);
                            if (user >= 0 && inChannel) {
                                gettimeofday(&users[user].lastData,NULL);
                                printDebug(users[user].useraddr);
                                printf("recv Request Say %s \"%s\"\n", reqSay.req_channel, reqSay.req_text);
                                //send to users
                                say(&txtSay, reqSay.req_text, users[user].username, channels[channel].channelname);
                                for (int i = 0; i < channels[channel].nUsers; i++) {
                                    if (sendto(sockfd, &txtSay, sizeof(txtSay), 0, (struct sockaddr *)&channels[channel].channelUsers[i].useraddr, sizeof(channels[channel].channelUsers[i].useraddr)) < 0) {
                                        printf( "SEND ERROR\n");
                                    }
                                }
                                //send to channels
                                s2sSay(&reqS2sSay, reqSay.req_text, users[user].username, channels[channel].channelname);
                                for (int i = 0; i < channels[channel].nNeighbors; i++) {
                                    if (sendto(sockfd, &reqS2sSay, sizeof(reqS2sSay), 0, (struct sockaddr *)&channels[channel].channelNeighbors[i], sizeof(struct sockaddr)) < 0) {
                                        printf("%i broke in say", ntohs(channels[channel].channelNeighbors[i].sin_port));
                                        perror("lol error: ");
                                    }
                                    printDebug(channels[channel].channelNeighbors[i]);
                                    printf("send s2s Say %s %s \"%s\"\n", users[user].username, reqSay.req_channel, reqSay.req_text);
                                }
                                //add identifier
                                addTag(reqS2sSay.req_id);

                            } else if (!inChannel) {
                                gettimeofday(&users[user].lastData,NULL);
                                serverError(&txtError, (char *)"Channel does not exist or you are not in channel.\n");
                                if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&users[user].useraddr, sizeof(users[user].useraddr)) < 0) {
                                    printf("SEND ERROR\n");
                                }
                                printf("%s not in channel %s.\n", users[user].username, channels[channel].channelname);
                            }
                        }
                        
                        break;
                        
                    case REQ_S2S_SAY :
                        if (recvfrom(sockfd, &reqS2sSay, sizeof(reqS2sSay), 0, (struct sockaddr *) &clientaddr, &len) < 0) {
                            printf("SAY RECV ERROR\n");
                            serverError(&txtError, (char *)"Problem receiving message. Please resend.\n");
                            if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                printf("SEND ERROR\n");
                            }
                        } else {
                            printDebug(clientaddr);
                            printf("recv s2s Say %s %s \"%s\"\n", reqS2sSay.req_username, reqS2sSay.req_channel, reqS2sSay.req_text);
                            
                            int channel = validChannel(reqS2sSay.req_channel, numChannels);
                            //if we are in the channel
                            if (channel < numChannels) {
                                //check for duplicates
                                //tagPrint(serveraddr);
                                if (checkTags(reqS2sSay.req_id)) {
                                    s2sLeave(&reqS2sLeave, reqS2sSay.req_channel);
                                    if (sendto(sockfd, &reqS2sLeave, sizeof(reqS2sLeave), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                        printf("SEND ERROR");
                                    }
                                    //memset(&channels[channel], 0, sizeof(aChannel));
                                    printDebug(clientaddr);
                                    printf("send s2s Leave %s\n", reqS2sSay.req_channel);
                                } else {
                                    addTag(reqS2sSay.req_id);
                                    //case 1: at least one user in channel, send to user
                                    for (int i = 0; i < channels[channel].nUsers; i++) {
                                        say(&txtSay, reqS2sSay.req_text, reqS2sSay.req_username, channels[channel].channelname);
                                        if (sendto(sockfd, &txtSay, sizeof(txtSay), 0, (struct sockaddr *)&channels[channel].channelUsers[i].useraddr, sizeof(channels[channel].channelUsers[i].useraddr)) < 0) {
                                            printf( "SEND ERROR\n");
                                        }
                                        printDebug(channels[channel].channelUsers[i].useraddr);
                                        printf("send Say %s %s \"%s\"\n", reqS2sSay.req_username, reqS2sSay.req_channel, reqS2sSay.req_text);
                                    }
                                    //case 2: at least one neighbor who is not sending neighbor, send s2s
                                    //check for duplicate received messages
                                    for (int i = 0; i < channels[channel].nNeighbors; i++) {
                                        s2sSay(&reqS2sSay2, reqS2sSay.req_text, reqS2sSay.req_username, channels[channel].channelname, reqS2sSay.req_id);
                                        if (channels[channel].channelNeighbors[i].sin_addr.s_addr != clientaddr.sin_addr.s_addr ||
                                            channels[channel].channelNeighbors[i].sin_port != clientaddr.sin_port) {
                                            if (sendto(sockfd, &reqS2sSay, sizeof(reqS2sSay), 0, (struct sockaddr *)&channels[channel].channelNeighbors[i], sizeof(struct sockaddr)) < 0) {
                                                printf("%i broke in s2s say\n", ntohs(channels[channel].channelNeighbors[i].sin_port));
                                                perror("lol error: ");
                                            }
                                            printDebug(channels[channel].channelNeighbors[i]);
                                            printf("send s2s Say %s %s \"%s\"\n", reqS2sSay.req_username, reqS2sSay.req_channel, reqS2sSay.req_text);
                                        }
                                    }
                                    //case 3: no users and no neighbors besides sending neighbor, send leave msg
                                    if (channels[channel].nNeighbors == 1 && channels[channel].nUsers == 0) {
                                        s2sLeave(&reqS2sLeave, reqS2sSay.req_channel);
                                        if (sendto(sockfd, &reqS2sLeave, sizeof(reqS2sLeave), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                            printf("SEND ERROR");
                                        }
                                        //remove self from channels
                                        removeServerFromChannel(&serveraddr, channel);
                                        memset(&channels[channel], 0, sizeof(aChannel));
                                        numChannels --;
                                        printDebug(clientaddr);
                                        printf("send s2s Leave %s\n", reqS2sSay.req_channel);
                                    }
                                }
                            } else {
                                printf("s2s Say recevied for nonexistant channel\n");
                            }

                        }
                        break;
                        
                    case REQ_LIST :
                        if (recvfrom(sockfd, &reqList, sizeof(reqList), 0, (struct sockaddr *) &clientaddr, &len) < 0) {
                            printf("PEEK RECV ERROR\n");
                            serverError(&txtError, (char *)"Problem receiving list request. Please resend.\n");
                            if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                printf("SEND ERROR\n");
                            }
                        } else {
                            int user = validUser(&clientaddr, numUsers);
                            if (user >= 0 ) {
                                gettimeofday(&users[user].lastData,NULL);
                                printDebug(users[user].useraddr);
                                printf("recv Request List\n");
                                list(&txtList, numChannels);
                                if (sendto(sockfd, &txtList, sizeof(txtList) + numChannels*sizeof(channel_info), 0, (struct sockaddr *)&users[user].useraddr, sizeof(users[user].useraddr)) < 0) {
                                    printf("SEND ERROR\n");
                                }
                            }
                        }
                        break;
                        
                    case REQ_WHO :
                        if (recvfrom(sockfd, &reqWho, sizeof(reqWho), 0, (struct sockaddr *) &clientaddr, &len) < 0) {
                            printf("PEEK RECV ERROR\n");
                            serverError(&txtError, (char *)"Problem receiving who message. Please resend\n");
                            if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
                                printf("SEND ERROR\n");
                            }
                        } else {
                            int user = validUser(&clientaddr, numUsers);
                            int channel = validChannel(reqWho.req_channel, numChannels);
                            if (user >= 0) {
                                printDebug(users[user].useraddr);
                                printf("recv Request Who %s\n", reqWho.req_channel);
                                if (channel < numChannels) {
                                    gettimeofday(&users[user].lastData,NULL);
                                    who(&txtWho, channels[channel].nUsers, channel);
                                    if (sendto(sockfd, &txtWho, sizeof(txtWho) + channels[channel].nUsers*sizeof(user_info), 0, (struct sockaddr *)&users[user].useraddr, sizeof(users[user].useraddr)) < 0) {
                                        printf( "SEND ERROR\n");
                                    }
                                } else if (channel == numChannels) {
                                    gettimeofday(&users[user].lastData,NULL);
                                    printf("%s attempts to list users on non-existant channel %s\n", users[user].username, reqWho.req_channel);
                                    serverError(&txtError, (char *)"Channel does not exist.");
                                    if (sendto(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *)&users[user].useraddr, sizeof(users[user].useraddr)) < 0) {
                                        printf("SEND ERROR\n");
                                    }
                                }
                            }
                        }
                        break;
                        
                    case REQ_KEEP_ALIVE :
                        if (recvfrom(sockfd, &reqKeepAlive, sizeof(reqKeepAlive), 0, (struct sockaddr *) &clientaddr, &len) < 0) {
                            printf("KEEP ALIVE ERROR\n");
                        } else {
                            int user = validUser(&clientaddr, numUsers);
                            if (user >= 0) {
                                gettimeofday(&users[user].lastData,NULL);
                                printDebug(users[user].useraddr);
                                printf("recv Request KeepAlive\n");
                            }
                        }
                        break;
                    
                    default :
                        if (recvfrom(sockfd, NULL, 0, 0, (struct sockaddr *) &clientaddr, &len) < 0) {
                            printf("RECEIVE ERROR\n");
                        } else {
                            printf("Unknown message type. Discarding message\n");
                        }
                }
                fflush(stdout);

            }
            fflush(stdout);
        }
    }
    return 0;
}
