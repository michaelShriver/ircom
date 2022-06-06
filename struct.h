#pragma once

#include <stdio.h>
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
    char nick[128];
    char username[128];
    char realname[128];
    int port;
    char *active_channel;
    int buffer_count;
    bufptr *server_buffer;
    bufline *buffer_read_ptr;
    struct termios termstate;
    struct termios termstate_raw;
    struct winsize ttysize;
    bool input_wait;
    bool output_wait;
    time_t nickwidth_set_at;
    FILE *pager;
};