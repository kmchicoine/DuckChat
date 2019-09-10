//
//  client.h
//  Client
//
//  Created by Kaley Chicoine on 10/13/17.
//  Copyright © 2017 Kaley Chicoine. All rights reserved.
//

#ifndef client_h
#define client_h

#define UNUSED __attribute__((unused))
#define REQ_BAD -1
#define REQ_SWITCH 8
#define SUBSCRIBED_MAX 10
#define TIMEOUT 60

#include "duckchat.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>

struct peek {
    int txtNum;
    int size;
};

int sockfd;
struct sockaddr_in myaddr;
struct sockaddr_in serveraddr;
struct itimerval newItVal, oldItVal;

void error(char *msg);
static void onalarm(UNUSED int sig);
void login(struct request_login* login, char* username);
void logout(struct request_logout* logout);
void join(struct request_join* join, char* channel);
void leave(struct request_leave* leave, char* channel);
void who(struct request_who* who, char* channel);
void say(struct request_say* say, char* channel, char* message);
void list(struct request_list* list);
void keepAlive(struct request_keep_alive* keepAlive);
int parseInput(char* input);
int match(char channelList[SUBSCRIBED_MAX][CHANNEL_MAX], char* leaveChannel, int totalChannels);
void remove(char channelList[SUBSCRIBED_MAX][CHANNEL_MAX], int remIndex, int totalChannels);
void setAlarm(bool firstCall);
static void onalarm(UNUSED int sig);

#endif /* client_h */
