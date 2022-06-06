#pragma once

#include "functions.h"

void *irc_event_loop(void *);
void dump_event(irc_session_t *, const char *, const char *, const char **, unsigned int);
void event_join(irc_session_t *, const char *, const char *, const char **, unsigned int);
void event_part(irc_session_t *, const char *, const char *, const char **, unsigned int);
void event_quit(irc_session_t *, const char *, const char *, const char **, unsigned int);
void event_kick(irc_session_t *, const char *, const char *, const char **, unsigned int);
void event_topic(irc_session_t *, const char *, const char *, const char **, unsigned int);
void event_connect(irc_session_t *, const char *, const char *, const char **, unsigned int);
void event_privmsg(irc_session_t *, const char *, const char *, const char **, unsigned int);
void event_notice(irc_session_t *, const char *, const char *, const char **, unsigned int);
void event_channel(irc_session_t *, const char *, const char *, const char **, unsigned int);
void event_action(irc_session_t *, const char *, const char *, const char **, unsigned int);
void event_numeric(irc_session_t *, unsigned int, const char *, const char **, unsigned int);