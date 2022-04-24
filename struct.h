#pragma once

#include <stdbool.h>
#include <termios.h>
#include <sys/ioctl.h>

typedef struct nickname nickname;

struct nickname
{
    char mode;
    char *handle;
    struct nickname *next;
};

typedef struct bufline bufline;

struct bufline
{
    struct bufline *prev;
    char *message;
    bool isread;
    struct bufline *next;
};

typedef struct bufptr bufptr;

struct bufptr
{
    struct bufptr *prevbuf;
    char *channel;
    char *topic;
    char *topicsetby;
    int nickwidth;
    int nickcount;
    bufline *head;
    bufline *curr;
    struct nickname *nicklist;
    struct bufptr *nextbuf;
};

typedef struct irc_ctx_t irc_ctx_t;

struct irc_ctx_t
{
    char *nick;
    char *active_channel;
    bufptr *buffer_index;
    int buffer_count;
};

extern bufptr *server_buffer;
extern bufline *buffer_read_ptr;
extern struct termios termstate;
extern struct winsize ttysize;
extern bool input_wait;
extern time_t nickwidth_set_at;
