#pragma once

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "pthread.h"
#include "libircclient.h"
#include "struct.h"
#include "arguments.h"

bufline *add_to_buffer(bufptr *, char *);
bufptr *init_buffer(irc_session_t *, const char *);
bufptr *init_server_buffer(irc_session_t *, char *);
nickname *init_nickentry();
bufptr *channel_buffer(irc_session_t *, const char *);
char *get_input();
int channel_isjoined(irc_session_t *, const char *);
int nick_is_member(irc_session_t *, char *, char *);
int add_member(irc_session_t *, const char *, const char *);
int delete_member(irc_session_t *, const char *, const char *);
int nickwidth_timer(irc_session_t *, char *);
void send_message(irc_session_t *);
void send_action(irc_session_t *);
void send_privmsg(irc_session_t *);
void kick_user(irc_session_t *);
void rewind_buffer(irc_session_t *, int);
void peek_channel(irc_session_t *);
void print_new_messages(irc_session_t *);
void exit_cleanup();
void reset_nicklist(irc_session_t *, char *);
void clear_nicklist(nickname *);
void clear_msglist(bufline *);
void clear_all(bufptr *);
void clear_buffer(irc_session_t *, bufptr *);