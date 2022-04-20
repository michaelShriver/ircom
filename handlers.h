#pragma once

#include "functions.h"

void *irc_event_loop(void *);
void dump_event();
void event_numeric();
void event_join();
void event_part();
void event_quit();
void event_kick();
void event_topic();
void event_connect();
void event_privmsg();
void event_channel();
void event_action();