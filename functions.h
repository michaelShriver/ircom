#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
//#include <sys/select.h>
#include "pthread.h"
#include "libircclient.h"
#include "struct.h"

void addlog(const char *, ...);
bufline *add_to_buffer();
bufptr *init_buffer();
bufptr *channel_buffer();
int channel_isjoined();
char *get_input();
void send_message();
void send_action();
void send_privmsg();
void show_prompt();
void exit_cleanup();
void rewind_buffer();
void peek_channel();
void print_new_messages();
void clear_all();
void clear_buffer();
