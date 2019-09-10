//
//  server.h
//  Client
//
//  Created by Kaley Chicoine on 10/28/17.
//  Copyright © 2017 Kaley Chicoine. All rights reserved.
//

#ifndef server_h
#define server_h

#define UNUSED __attribute__((unused))

#define USERS_MAX 20
#define NCHANNEL_MAX 20
#define TIMEOUT2 120
#define TIMEOUT1 60

#include "duckchat.h"
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

struct aUser {
    char username[USERNAME_MAX+1];
    struct sockaddr_in useraddr;
    int nChannels;
    char userChannels[NCHANNEL_MAX][CHANNEL_MAX+1];
    struct timeval lastData;
};
struct aChannel {
    char channelname[CHANNEL_MAX+1];
    struct aUser channelUsers[USERS_MAX+1];
    int nUsers;
    int nNeighbors;
    struct sockaddr_in channelNeighbors[NCHANNEL_MAX];
};
struct aNeighbor {
    struct sockaddr_in neighboraddr;
    struct timeval lastData;
};

struct aUser users[USERS_MAX];
struct aChannel channels[NCHANNEL_MAX];
int numUsers = 0,numChannels = 0, numNeighbors = 0, numMessages = 0;
struct aNeighbor neighbors[20];
long long messageTags[1000];
struct itimerval newItVal2User, oldItVal2User;
struct sockaddr_in serveraddr;
int serverPort, sockfd;
struct itimerspec newItVal1, oldItVal1, newItVal2Server, oldItVal2Server;
timer_t timerid1, timerid2;




int validUser(struct sockaddr_in* test, int numUsers);
int validChannel(char *channelName, int numChannels);
void removeChannelFromUser(struct aUser* user, int remIndex, int totalChannels);
void removeUserFromChannel(struct aChannel* channel, int remIndex, int totalUsers);
int leave(struct aUser* user, struct aChannel* channel);
int removeDuplicateChannels(int numChannels);
int removeUser(int numUsers, int user);
bool checkUserInChannel(struct aUser* user, struct aChannel* channel);
void serverError(struct text_error* txtError, char* message);
void say(struct text_say* txtSay, char* message, char* user, char* channel);
void list(struct text_list* txtList, int numChannels);
void who(struct text_who* txtWho, int numUsers, int channel);
void removeUserFromAllChannels(int user);
static void onTwoAlarmUser(UNUSED int sig);
static void onOneAlarm(UNUSED int sig);
static void onTwoAlarmServer(UNUSED int sig);


#endif














