#pragma once

#include "functions.h"

void *irc_event_loop(void *);
void dump_event();
void event_numeric();
void event_join();
void event_part();
void event_connect();
void event_privmsg();
void event_channel();
void event_action();
void dcc_recv_callback();
void dcc_file_recv_callback();
