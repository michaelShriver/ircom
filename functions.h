#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "pthread.h"
#include "libircclient.h"
#include "struct.h"

void addlog(const char *, ...);
bufline *add_to_buffer();
bufptr *init_buffer();
nickname *init_nickentry();
bufptr *channel_buffer();
char *get_input();
int channel_isjoined();
int nick_is_member();
void add_member();
void delete_member();
void send_message();
void send_action();
void send_privmsg();
void show_prompt();
void exit_cleanup();
void rewind_buffer();
void peek_channel();
void print_new_messages();
void clear_nicklist();
void clear_all();
void clear_buffer();
