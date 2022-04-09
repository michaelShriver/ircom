#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdbool.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "pthread.h"
#include "libircclient.h"
#include "struct.h"

void addlog(const char *, ...);
bufline *add_to_buffer();
bufptr *init_buffer();
bufptr *channel_buffer();
int channel_isjoined();
int kbhit();
char *get_input();
void send_message();
void send_action();
void show_prompt();
void reset_termstate();
void rewind_buffer();
void peek_channel();
void print_message_buffer();
void clear_buffer();
void clear_all();
