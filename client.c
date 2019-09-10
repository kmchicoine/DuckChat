//
//  client.c
//  Client
//
//  Created by Kaley Chicoine on 10/13/17.
//  Copyright © 2017 Kaley Chicoine. All rights reserved.
//

#include "client.h"
#include "raw.h"
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>
#include <stdint.h>

void error(char *msg) {
    printf("%s",msg);
}
void login(struct request_login* login, char* username) {
    login->req_type = REQ_LOGIN;
    strcpy(login->req_username, username);
}
void logout(struct request_logout* logout) {
    logout->req_type = REQ_LOGOUT;
}
void join(struct request_join* join, char* channel) {
    join->req_type = REQ_JOIN;
    strcpy(join->req_channel, channel);
}
void leave(struct request_leave* leave, char* channel) {
    leave->req_type = REQ_LEAVE;
    strcpy(leave->req_channel, channel);
}
void who(struct request_who* who, char* channel) {
    who->req_type = REQ_WHO;
    strcpy(who->req_channel, channel);
}
void say(struct request_say* say, char* channel, char* message) {
    say->req_type = REQ_SAY;
    strcpy(say->req_channel, channel);
    strcpy(say->req_text, message);
}
void list(struct request_list* list) {
    list->req_type = REQ_LIST;
}
void keepAlive(struct request_keep_alive* keepAlive) {
    keepAlive->req_type = REQ_KEEP_ALIVE;
}
int parseInput(char* input) {
    if(input[0] == '/') {
        if(strcmp(input, "/exit") == 0) {
            return REQ_LOGOUT;
        }
        else if(strncmp(input, "/join ", sizeof("/join ")-1) == 0) {
            return REQ_JOIN;
        }
        else if(strncmp(input, "/leave ", sizeof("/leave ")-1) == 0) {
            return REQ_LEAVE;
        }
        else if(strcmp(input, "/list") == 0) {
            return REQ_LIST;
        }
        else if(strncmp(input, "/who ", sizeof("/who ")-1) == 0) {
            return REQ_WHO;
        }
        else if(strncmp(input, "/switch ", sizeof("/switch ")-1) == 0) {
            return REQ_SWITCH;
        }
        else
            return REQ_BAD;
    }
    else
        return REQ_SAY;
}
int match(char channelList[SUBSCRIBED_MAX][CHANNEL_MAX], char* leaveChannel, int totalChannels) {
    for (int i = 0; i < totalChannels; i++) {
        if (strcmp(channelList[i], leaveChannel) == 0)
            return i;
    }
    return -1;
}
void remove(char channelList[SUBSCRIBED_MAX][CHANNEL_MAX], int remIndex, int totalChannels) {
    for (int i = remIndex; i < totalChannels-1; i++) {
        strcpy(channelList[i], channelList[i+1]);
    }
    memset(channelList[totalChannels], 0, CHANNEL_MAX);
}
void setAlarm(bool firstCall) {
    //save old timer
    oldItVal = newItVal;
    if (signal(SIGALRM, onalarm) == SIG_ERR) {
        char msg[50] = "Could not catch alarm";
        error(msg);
    }
    //set new timer
    newItVal.it_value.tv_sec = TIMEOUT;
    newItVal.it_value.tv_usec = 0;
    newItVal.it_interval = newItVal.it_value;
    //reset old timer if not first call
    if (firstCall) {
        if (setitimer(ITIMER_REAL, &newItVal, NULL) == -1) {
            char msg[50] = "error calling setitimer()";
            error(msg);
        }
    } else {
        if (setitimer(ITIMER_REAL, &newItVal, &oldItVal) == -1) {
            char msg[50] = "error calling setitimer()";
            error(msg);
        }
    }
}
static void onalarm(UNUSED int sig) {
    struct request_keep_alive reqKeepAlive;
    keepAlive(&reqKeepAlive);
    if (sendto(sockfd, &reqKeepAlive, sizeof(reqKeepAlive), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        char msg[] = "SEND ERROR";
        error(msg);
    }
    setAlarm(false);
}
//TODO: put numUsers, user list in header file
int main(int argc, char *argv[] ) {
    int serverPort;
    char hostname[UNIX_PATH_MAX];
    char username[USERNAME_MAX];
    struct hostent *hp;
    char input[SAY_MAX + 1];
    struct request_login reqLogin;
    struct request_logout reqLogout;
    struct request_join reqJoin;
    struct request_who reqWho;
    struct request_leave reqLeave;
    struct request_list reqList;
    struct request_say reqSay;
    struct text_who txtWho;
    struct text_error txtError;
    struct text_list txtList;
    struct text_say txtSay;
    int command = REQ_JOIN;
    int activeChannel = 0;
    int totalChannels = 0;
    char channels[SUBSCRIBED_MAX][CHANNEL_MAX];
    fd_set readset;
    int maxsd, msgPeek, count = 0;
    struct peek numPeek;
    socklen_t len;
    char test;
    
    //set to raw mode
    if(raw_mode() < 0) {
        printf( "Failed to enter raw mode. Please restart.\n");
        return 1;
    }
    atexit(cooked_mode);
    
    //start keep alive alarm
    setAlarm(true);
    
    //check inputs
    if (strlen(argv[1]) > UNIX_PATH_MAX) {
        printf("Path too long: %i characters.\n", int(strlen(argv[1])));
        return 1;
    }
    else {
        strcpy(hostname, argv[1]);
    }
    serverPort = strtol(argv[2], NULL, 10);
    if (strlen(argv[3]) > USERNAME_MAX) {
        printf("Username too long: %i characters. \n", int(strlen(argv[3])));
        return 1;
    }
    else {
        strcpy(username, argv[3]);
    }
    
    //set up socket
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        printf("SOCKET ERROR\n");
        return 1;
    }
    //set my socket info
    memset((char *) &myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_port = htons(0);
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    //bind name to socket
    if(bind(sockfd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        printf( "BIND ERROR\n");
        return 1;
    }
    
    //DNS lookup
    hp = gethostbyname(hostname);
    if (!hp) {
        char msg[] = "HOST ERROR: Could not obtain address";
        error(msg);
    }
    
    //set server socket info
    memset((char *) &serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, hp->h_addr_list[0], &serveraddr.sin_addr);
    
    //login
    login(&reqLogin, username);
    if (sendto(sockfd, &reqLogin, sizeof(reqLogin), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        char msg[] = "SEND ERROR";
        error(msg);
    } else {
        setAlarm(false);
    }
    
    //create message to join Common
    strcpy(channels[totalChannels], "Common");
    join(&reqJoin, channels[totalChannels]);
    totalChannels ++;
    
    //Join Common
    if (sendto(sockfd, &reqJoin, sizeof(reqJoin), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        char msg[] = "JOIN ERROR";
        error(msg);
    } else {
        setAlarm(false);
    }
    printf("> ");
    fflush(stdout);
    while (command != REQ_LOGOUT) {
        //set fd's to listen for
        FD_ZERO(&readset);
        FD_SET(fileno(stdin), &readset);
        FD_SET(sockfd, &readset);
        if (fileno(stdin) > sockfd) {
            maxsd = fileno(stdin);
        } else {
            maxsd = sockfd;
        }
        
        //listen for user/server input and continue through alarm signal
        if (select(maxsd + 1, &readset, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                //send keep alive
                continue;
            } else {
                printf("Select error.\n");
            }
        }
        
        //handle user input
        if (FD_ISSET(fileno(stdin), &readset)) {
            test = fgetc(stdin);
            if (test != '\n') {
                if (count < SAY_MAX) {
                    printf("%c",test);
                    fflush( stdout );
                    strncpy(&input[count],&test,1);
                    count ++;
                }
            } else {
                printf("\n");
                strcpy(&input[count], "\0");
                count = 0;
                //parse command from input
                command = parseInput(input);
                
                //execute command
                switch(command)  {
                    case REQ_LOGOUT :
                        logout(&reqLogout);
                        if (sendto(sockfd, &reqLogout, sizeof(reqLogout), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
                            char msg[] = "LOGOUT ERROR";
                            error(msg);
                        } else {
                            setAlarm(false);
                        }
                        break;
                    case REQ_JOIN :
                        if (totalChannels == SUBSCRIBED_MAX) {
                            printf("Max channels reached. Must leave channel to join another channel.\n");
                        }
                        else if (input[6] == 0) {
                            printf("Must enter channel name to join.\n");
                        } else if (strlen(&input[6]) > CHANNEL_MAX + 1) {
                            printf("Channel name too long.\n");
                        } else {
                            join(&reqJoin, &input[6]);
                            if (sendto(sockfd, &reqJoin, sizeof(reqJoin), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
                                char msg[] = "JOIN ERROR";
                                error(msg);
                            } else {
                                setAlarm(false);
                            }
                            activeChannel = totalChannels;
                            strcpy(channels[totalChannels], &input[6]);
                            totalChannels ++;
                        }
                        break;
                    case REQ_LEAVE :
                        if (input[7] == 0) {
                            printf("Must enter channel name to leave.\n");
                        } else if (strlen(&input[7]) > CHANNEL_MAX + 1) {
                            printf("Channel name too long.\n");
                        } else if (match(channels, &input[7], totalChannels) < 0) {
                            printf("You are not in channel %s\n", &input[7]);
                        } else {
                            input[strcspn(input, "\n")] = 0;
                            leave(&reqLeave, &input[7]);
                            if (sendto(sockfd, &reqLeave, sizeof(reqLeave), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
                                char msg[] = "LEAVE ERROR";
                                error(msg);
                            } else {
                                setAlarm(false);
                            }
                            remove(channels, match(channels, &input[7], totalChannels), totalChannels);
                            totalChannels --;
                        }
                        break;
                    case REQ_LIST :
                        list(&reqList);
                        if (sendto(sockfd, &reqList, sizeof(reqList), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
                            char msg[] = "LIST ERROR";
                            error(msg);
                        } else {
                            setAlarm(false);
                        }
                        break;
                    case REQ_WHO :
                        if (input[5] == 0) {
                            printf("Must enter channel name to who.\n");
                        } else if (strlen(&input[7]) > CHANNEL_MAX + 1) {
                            printf("Channel name too long.\n");
                        } else {
                            who(&reqWho,&input[5]);
                            if (sendto(sockfd, &reqWho, sizeof(reqWho), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
                                char msg[] = "WHO ERROR";
                                error(msg);
                            } else {
                                setAlarm(false);
                            }
                        }
                        break;
                    case REQ_SWITCH :
                        if (input[8] == 0) {
                            printf("Must enter channel name to switch to.\n");
                        } else if (strlen(&input[8]) > CHANNEL_MAX + 1) {
                            printf("Channel name too long.\n");
                        } else if (match(channels, &input[8], totalChannels) < 0) {
                            printf("You are not in channel %s\n", &input[8]);
                        } else {
                            activeChannel = match(channels, &input[8], totalChannels);
                        }
                        
                        break;
                    case REQ_SAY:
                        if (strlen(input) > SAY_MAX) {
                            printf("Message too long. Max length = 64.\n");
                        } else {
                            say(&reqSay, channels[activeChannel], input);
                            if (sendto(sockfd, &reqSay, sizeof(reqSay), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
                                char msg[] = "SAY ERROR";
                                error(msg);
                            } else {
                                setAlarm(false);
                            }
                        }
                        break;
                    default :
                        printf("Bad Command. Must enter channel name for join, leave, who, switch.\n");
                        break;
                }
                memset(input, 0, sizeof(input));
                printf("> ");
                fflush(stdout);
            }
        }
        if (FD_ISSET(sockfd, &readset)){
            for (int i = 0; i < (int)strlen(input) + 5; i++) {
                printf("\b");
            }
            len = sizeof(serveraddr);
            
            if (recvfrom(sockfd, &msgPeek, sizeof(msgPeek), MSG_PEEK, (struct sockaddr *) &serveraddr, &len) < 0) {
                char msg[] = "PEEK RECV ERROR";
                error(msg);
            } else {
                switch (msgPeek) {
                    case TXT_SAY :
                        if (recvfrom(sockfd, &txtSay, sizeof(txtSay), 0, (struct sockaddr *) &serveraddr, &len) < 0) {
                            char msg[] = "RECV ERROR";
                            error(msg);
                        } else {
                            printf("[%s][%s]: %s\n", txtSay.txt_channel, txtSay.txt_username,txtSay.txt_text);
                        }
                        break;
                    case TXT_LIST :
                        if (recvfrom(sockfd, &numPeek, sizeof(numPeek), MSG_PEEK, (struct sockaddr *) &serveraddr, &len) < 0) {
                            char msg[] = "PEEK RECV ERROR";
                            error(msg);
                        } else {
                            if (recvfrom(sockfd, &txtList, sizeof(txtList) + sizeof(channel_info)*numPeek.size, 0, (struct sockaddr *) &serveraddr, &len) < 0) {
                                char msg[] = "RECV ERROR";
                                error(msg);
                            } else {
                                printf("Existing Channels:\n");
                                for(int i = 0; i < txtList.txt_nchannels; i++)
                                    printf(" %s\n",txtList.txt_channels[i].ch_channel);
                            }
                        }
                        break;
                    case TXT_WHO :
                        if (recvfrom(sockfd, &numPeek, sizeof(numPeek), MSG_PEEK, (struct sockaddr *) &serveraddr, &len) < 0) {
                            char msg[] = "PEEK RECV ERROR";
                            error(msg);
                        } else {
                            if (recvfrom(sockfd, &txtWho, sizeof(txtWho) + sizeof(user_info)*numPeek.size, 0, (struct sockaddr *) &serveraddr, &len) < 0) {
                                char msg[] = "RECV ERROR";
                                error(msg);
                            } else {
                                printf("Users on channel %s: \n", txtWho.txt_channel);
                                for(int i = 0; i < txtWho.txt_nusernames; i++)
                                    if (strlen(txtWho.txt_users[i].us_username) > USERNAME_MAX) {
                                        printf(" ");
                                        for (int j = 0; j < USERNAME_MAX; j++ ) {
                                            printf("%c",txtWho.txt_users[i].us_username[j]);
                                        }
                                        printf("\n");
                                    } else {
                                        printf(" %s\n",txtWho.txt_users[i].us_username);
                                    }
                            }
                        }
                        break;
                    case TXT_ERROR:
                        if (recvfrom(sockfd, &txtError, sizeof(txtError), 0, (struct sockaddr *) &serveraddr, &len) < 0) {
                            char msg[] = "RECV ERROR";
                            error(msg);
                        } else {
                            printf("Error: %s\n", txtError.txt_error);
                        }
                        break;
                    default:
                        
                        if (recvfrom(sockfd, NULL, 0, 0, (struct sockaddr *) &serveraddr, &len) < 0) {
                            char msg[] = "RECV ERROR";
                            error(msg);
                        } else {
                            printf("Unknown message type from server. Discarding.\n");
                        }
                        break;
                }
            }
            
            printf("> %s", input);
            fflush( stdout );
        }
    }
    return 0;
}



