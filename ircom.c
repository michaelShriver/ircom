#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "pthread.h"
#include "libircclient.h"

/* Data structures */

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
    bufline *head;
    bufline *curr;
    struct bufptr *nextbuf;
};

typedef struct
{
    char *nick;
    char *initial_chan;
    char active_channel[128];
    bufptr *buffer_index;
    int buffer_count;
} irc_ctx_t;

bufptr *server_buffer;
bufline *buffer_read_ptr;
struct termios termstate;
struct winsize ttysize;

/* Functions */

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
void addlog(const char *, ...);

void *irc_event_loop(void *);
bufline *add_to_buffer();
bufptr *init_buffer();
bufptr *channel_buffer();
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
void delete_buffer();

int main(int argc, char **argv)
{
    if ( argc != 4 )
    {
        printf ("Usage: %s <server> <nick> <channel>\n", argv[0]);
        return 1;
    }

    /* Initialize callbacks and pointers */
    irc_callbacks_t callbacks;
    irc_session_t * sess;
    unsigned short port = 6667;
    //char current_channel[128]; //TODO: Add mechanism to update this when on external channel joins
    char keycmd;
    pthread_t event_thread;

    /* Save terminal state, and restore on exit */
    ioctl(0, TIOCGWINSZ, &ttysize);
    struct termios termstate_raw;
    tcgetattr(0, &termstate);
    memcpy(&termstate_raw, &termstate, sizeof(termstate_raw)); 
    atexit(reset_termstate);

    /* Zero out memory allocation for callbacks struct */
    memset (&callbacks, 0, sizeof(callbacks));

    callbacks.event_connect = event_connect;
    callbacks.event_join = event_join;
    callbacks.event_nick = dump_event;
    callbacks.event_quit = dump_event;
    callbacks.event_part = event_part;
    callbacks.event_mode = dump_event;
    callbacks.event_topic = dump_event;
    callbacks.event_kick = dump_event;
    callbacks.event_channel = event_channel;
    callbacks.event_privmsg = event_privmsg;
    callbacks.event_notice = dump_event;
    callbacks.event_invite = dump_event;
    callbacks.event_umode = dump_event;
    callbacks.event_ctcp_rep = dump_event;
    callbacks.event_ctcp_action = event_action;
    callbacks.event_unknown = dump_event;
    callbacks.event_numeric = event_numeric;

    //callbacks.event_dcc_chat_req = irc_event_dcc_chat;
    //callbacks.event_dcc_send_req = irc_event_dcc_send;

    /* Create a new IRC session */
    sess = irc_create_session(&callbacks);

    /* check for error */
    if ( !sess )
    {
        printf ("Could not create session\n");
        return 1;
    }

    /* Initialize buffers */
    server_buffer = init_buffer(argv[1]);

    /* Start output in the server buffer */
    buffer_read_ptr = server_buffer->curr;

    /* Initialize my user-defined IRC context, with two buffers */
    irc_ctx_t ctx;
    ctx.nick = argv[2];
    ctx.initial_chan = argv[3];
    strcpy(ctx.active_channel, ctx.initial_chan);

    irc_set_ctx(sess, &ctx);

    /* Initiate the IRC server connection */
    if ( irc_connect (sess, argv[1], port, 0, argv[2], 0, 0) )
    {
        printf ("Could not connect: %s\n", irc_strerror (irc_errno(sess)));
        return 1;
    }

    /* Fork thread that runs libirc event handling loop */
    int err = pthread_create(&event_thread, NULL, irc_event_loop, (void *)sess);
    if (err)
    {
        printf ("Failed to create thread\n");
        return 1;
    }

    /* Set termstate to raw */
    cfmakeraw(&termstate_raw);
    tcsetattr(0, TCSANOW, &termstate_raw);

    /* Wait for the message buffer to be initialized with valid data */
    while (buffer_read_ptr->message == NULL)
    {
    }

    /* This is where my input/output loop will go */
    while (1)
    {
        /* Walk the message buffer, and write messages to the terminal */
        while (buffer_read_ptr->next != NULL)
        {
            if (buffer_read_ptr->isread == 0 && buffer_read_ptr->message != NULL)
            {
                buffer_read_ptr->isread = 1;
                printf("%s\r\n", buffer_read_ptr->message);
            }

            if (buffer_read_ptr->next != NULL)
            {
                buffer_read_ptr = buffer_read_ptr->next;
            }
        }

        if (buffer_read_ptr->isread == 0 && buffer_read_ptr->message != NULL)
        {
            buffer_read_ptr->isread = 1;
            printf("%s\r\n", buffer_read_ptr->message);
        }

        /* check for user input, or add a little delay */
        if (!kbhit())
        {
            sleep(.1);
        }
        else
        {
            char stroke = getchar();

            if (stroke == 'q' || stroke == 3)
            {
                goto quit;
            }
            else if (stroke == 'r')
            {
                rewind_buffer(buffer_read_ptr, -1);
            }
            else if (stroke == 'R')
            {
                int lines;

                tcsetattr(0, TCSANOW, &termstate);
                printf(":lines> ");
                scanf("%d", &lines);
                tcsetattr(0, TCSANOW, &termstate_raw);
                (void)getchar();
                rewind_buffer(buffer_read_ptr, lines);
            }
            else if (stroke == 'e')
            {
                char *input;

                tcsetattr(0, TCSANOW, &termstate);
                printf(":emote> ");
                input = get_input();
                send_action(sess, input);
                free(input);
                tcsetattr(0, TCSANOW, &termstate_raw);
            }
            else if (stroke == 'g')
            {
                char *input;

                tcsetattr(0, TCSANOW, &termstate);
                printf(":channel> ");
                input = get_input();
                strcpy(ctx.active_channel, input);
                free(input);
                buffer_read_ptr = channel_buffer(ctx.active_channel)->curr;
                irc_cmd_join(sess, ctx.active_channel, 0);
                // TODO: determine if I am already joined to a channel, switch buffers appropriately
                tcsetattr(0, TCSANOW, &termstate_raw);
            }
            else if (stroke == 'p')
            {
                char *input;

                tcsetattr(0, TCSANOW, &termstate);
                printf(":peek> ");
                input = get_input();
                peek_channel(input);
                free(input);
                tcsetattr(0, TCSANOW, &termstate_raw);
            }
            else if (stroke == 'P')
            {
                tcsetattr(0, TCSANOW, &termstate);
                irc_cmd_part(sess, ctx.active_channel);
                tcsetattr(0, TCSANOW, &termstate_raw);
            }
            else
            {
                char *input;

                tcsetattr(0, TCSANOW, &termstate);
                show_prompt(ctx);
                input = get_input();
                if (strcmp(input, "") == 0)
                    printf("<no message sent>\n");
                else
                    send_message(sess, ctx.active_channel, input);
                free(input);
                tcsetattr(0, TCSANOW, &termstate_raw);
            }
        }
    }

quit:

    // TODO: create cleanup functions that walks buffers and frees mallocs
    tcsetattr(0, TCSANOW, &termstate);
	return 0;
}

/* Initialize a new channel message buffer, return a pointer to the new buffer */
bufptr *init_buffer(char *channel)
{
    char *bufchan = malloc(sizeof(char) * (strlen(channel) + 1));
    strcpy(bufchan, channel);

    bufptr *channel_buffer = malloc(sizeof(struct bufptr));
    channel_buffer->head = channel_buffer->curr = (struct bufline*) malloc(sizeof(struct bufline));
    channel_buffer->channel = bufchan;
    channel_buffer->curr->prev = NULL;
    channel_buffer->curr->next = NULL;
    channel_buffer->curr->message = NULL;
    channel_buffer->curr->isread = NULL;
    channel_buffer->prevbuf = NULL;
    channel_buffer->nextbuf = NULL;

    return channel_buffer;
}

/* Return a pointer for a specified channel's buffer, init a new buffer if needed */
bufptr *channel_buffer(char *channel)
{
    bufptr *search_ptr = server_buffer;

    while (search_ptr->nextbuf != NULL)
    {
        if(strcmp(search_ptr->nextbuf->channel, channel) == 0)
            return search_ptr->nextbuf;

        search_ptr = search_ptr->nextbuf;
    }

    bufptr *newbuf = init_buffer(channel);
    newbuf->prevbuf = search_ptr;
    search_ptr->nextbuf = newbuf;

    return search_ptr->nextbuf;
}

/* Add a message to the end of the message buffer, return a pointer to new entry */
bufline *add_to_buffer(bufline *msgbuffer, char *cmsg)
{
    char *messageline = (char*)malloc(sizeof(char) * (strlen(cmsg) + 1));
    strcpy(messageline, cmsg);

    /* Ensure we are at the end of the buffer */
    while (msgbuffer->next != NULL)
        msgbuffer = msgbuffer->next;

    /* Handle the first node in the message buffer */
    if (msgbuffer->message == NULL)
    {
        msgbuffer->prev = NULL;
        msgbuffer->next = NULL;
        msgbuffer->message = messageline;
        msgbuffer->isread = 0;

        return msgbuffer;
    }

    /* Add a new message to the end of the buffer */
    bufline *newmsg = (struct bufline*) malloc(sizeof(struct bufline));
    msgbuffer->next = newmsg;
    newmsg->prev = msgbuffer;
    newmsg->next = NULL;
    newmsg->message = messageline;
    newmsg->isread = 0;

    return newmsg;
}

/* Send a message to the channel, and add it to my buffer */
void send_message(irc_session_t *s, char *channel, char *message)
{
    irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (s);

    int size = sizeof(char) * (strlen(ctx->nick) + strlen(message) + 4);
    char *bufferline = (char *)malloc(size);
    bufptr *message_buffer = channel_buffer(channel); //CHANGE THIS AND BELOW

    snprintf(bufferline, size, "[%s] %s", ctx->nick, message);
    irc_cmd_msg(s, channel, message);
    message_buffer->curr = add_to_buffer(message_buffer->curr, bufferline);
    message_buffer->curr->isread = 1;
    free(bufferline);
}

/* Send an action to the channel, and add it to my buffer */
void send_action(irc_session_t *s, char *action)
{
    irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (s);

    int size = sizeof(char) * (strlen(ctx->nick) + strlen(action) + 4);
    char *bufferline = (char *)malloc(size);
    bufptr *message_buffer = channel_buffer(ctx->active_channel);

    snprintf(bufferline, size, "<%s %s>", ctx->nick, action);
    irc_cmd_me(s, ctx->active_channel, action);
    message_buffer->curr = add_to_buffer(message_buffer->curr, bufferline);
    message_buffer->curr->isread = 1;
    free(bufferline);
}

/* Run the event handling loop in it's own thread */
void *irc_event_loop(void * sess)
{
    irc_session_t * s;
    s = sess;

    if (irc_run(s))
    {
        printf ("Could not connect or I/O error: %s\n", irc_strerror (irc_errno(sess)));
        return NULL;
    }

    return NULL;
}

/* A placeholder function for events we have not implemented */
void dump_event (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    char buf[512];
    int cnt;

    buf[0] = '\0';
    strcat(buf, params[count-1]);
    addlog("%s: %s", event, buf);
}

/* Example functions from irctest.c, for POC */
void dcc_recv_callback (irc_session_t * session, irc_dcc_t id, int status, void * ctx, const char * data, unsigned int length)
{
    static int count = 1;
    char buf[12];

    switch (status)
    {
    case LIBIRC_ERR_CLOSED:
        printf ("DCC %d: chat closed\n", id);
        break;

    case 0:
        if ( !data )
        {
            printf ("DCC %d: chat connected\n", id);
            irc_dcc_msg (session, id, "Hehe");
        }
        else
        {
            printf ("DCC %d: %s\n", id, data);
            sprintf (buf, "DCC [%d]: %d", id, count++);
            irc_dcc_msg (session, id, buf);
        }
        break;

    default:
        printf ("DCC %d: error %s\n", id, irc_strerror(status));
        break;
    }
}

void dcc_file_recv_callback (irc_session_t * session, irc_dcc_t id, int status, void * ctx, const char * data, unsigned int length)
{
    if ( status == 0 && length == 0 )
    {
        printf ("File sent successfully\n");

        if ( ctx )
            fclose ((FILE*) ctx);
    }
    else if ( status )
    {
        printf ("File sent error: %d\n", status);

        if ( ctx )
            fclose ((FILE*) ctx);
    }
    else
    {
        if ( ctx )
            fwrite (data, 1, length, (FILE*) ctx);
        printf ("File sent progress: %d\n", length);
    }
}

/* Log undefined events to a text file and the server buffer */
void addlog (const char * fmt, ...)
{
    FILE * fp;
    char buf[1024];
    va_list va_alist;

    va_start (va_alist, fmt);
    vsnprintf (buf, sizeof(buf), fmt, va_alist);
    va_end (va_alist);

    server_buffer->curr = add_to_buffer(server_buffer->curr, buf);

    if ( (fp = fopen ("irctest.log", "ab")) != 0 )
    {
        fprintf (fp, "%s\n", buf);
        fclose (fp);
    }
}

void event_join (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    char nickbuf[128];
    char chanbuf[128];

    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    strcpy(chanbuf, params[0]);
    irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (session);
    bufptr *message_buffer = channel_buffer(chanbuf);

    if(strcmp(nickbuf, ctx->nick) == 0)
    {
        buffer_read_ptr = message_buffer->curr; // Move read pointer to first channel buffer upon join
        irc_cmd_user_mode (session, "+i");
    } 

    char joinmsg[256];
    snprintf(joinmsg, 256, "%s has joined %s.", nickbuf, chanbuf);
    message_buffer->curr = add_to_buffer(message_buffer->curr, joinmsg);
    message_buffer->curr->isread = strcmp(ctx->active_channel, chanbuf) == 0 ? 0 : 1;
}

void event_part(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    char nickbuf[128];
    char chanbuf[128];

    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    strcpy(chanbuf, params[0]);
    irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (session);
    bufptr *message_buffer = channel_buffer(chanbuf);

    if(strcmp(nickbuf, ctx->nick) == 0)
    {
        clear_buffer(channel_buffer(chanbuf));
    }
    else
    {
        char joinmsg[256];
        snprintf(joinmsg, 256, "%s has left %s.", nickbuf, params[0]);
        message_buffer->curr = add_to_buffer(message_buffer->curr, joinmsg);
        message_buffer->curr->isread = strcmp(ctx->active_channel, chanbuf) == 0 ? 0 : 1;
    }

    return;
}

void event_connect (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (session);
    dump_event (session, event, origin, params, count);

    if(ctx->initial_chan != NULL)
        irc_cmd_join (session, ctx->initial_chan, 0);
}

void event_channel (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (session);
    char nickbuf[128];
    char chanbuf[128];
    char msgbuf[1024];
    char messageline[1156];

    if ( count != 2 )
        return;

    if ( !origin )
        return;

    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    strcpy(msgbuf, params[1]);
    strcpy(chanbuf, params[0]);
    bufptr *message_buffer = channel_buffer(chanbuf);

    snprintf(messageline, 1156, "[%s] %s", nickbuf, msgbuf);
    message_buffer->curr = add_to_buffer(message_buffer->curr, messageline);
    message_buffer->curr->isread = strcmp(ctx->active_channel, chanbuf) == 0 ? 0 : 1;

    return;
}

void event_action(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    char nickbuf[128];
    char actionbuf[256];
    char chanbuf[128];

    strcpy(chanbuf, params[0]);
    bufptr *message_buffer = channel_buffer(chanbuf);

    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    snprintf(actionbuf, 256, "<%s %s>", nickbuf, params[1]);
    message_buffer->curr = add_to_buffer(message_buffer->curr, actionbuf);
}

void event_privmsg (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    dump_event (session, event, origin, params, count);

    printf ("'%s' said me (%s): %s\n",
        origin ? origin : "someone",
        params[0], params[1] );
}

void event_numeric (irc_session_t * session, unsigned int event, const char * origin, const char ** params, unsigned int count)
{
    char buf[24];
    sprintf (buf, "%d", event);

    dump_event (session, buf, origin, params, count);
}

/* Implement non-blocking character input kbhit() */
int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
}

void reset_termstate()
{
    tcsetattr(0, TCSANOW, &termstate);
}

/* custom functions for testing */
void show_prompt(irc_ctx_t ctx)
{
    printf("[%s] ", ctx.nick);
}

char * get_input()
{
    char *input = NULL;
    size_t buffer_size = 0;

    getline(&input, &buffer_size, stdin);
    input[strlen(input)-1] = '\0'; // Remote trailing newline

    return input;
}

void rewind_buffer(bufline *buffer_read_ptr, int lines)
{
    ioctl(0, TIOCGWINSZ, &ttysize); //TODO: change output based on term size
                                   
    if (lines <= 0)
    {
        lines = 0;
        while(lines < (ttysize.ws_row - 4))
        {
            if (buffer_read_ptr->prev == NULL)
                break; 
            lines = lines + (((strlen(buffer_read_ptr->message)) / ttysize.ws_col) + 1);
            buffer_read_ptr = buffer_read_ptr->prev;
        }
        if (buffer_read_ptr->next != NULL)
            buffer_read_ptr = buffer_read_ptr->next;
    }
    else
    {
        for (lines--; lines > 0; lines--)
        {
            if (buffer_read_ptr->prev == NULL)
               break; 
            buffer_read_ptr = buffer_read_ptr->prev;
        }
    }

    printf("--Beginning-Review--------------------------------------------------------------\r\n");
    while (buffer_read_ptr->next != NULL)
    {
        printf("%s\r\n", buffer_read_ptr->message);
        buffer_read_ptr = buffer_read_ptr->next;
    }
    printf("%s\r\n", buffer_read_ptr->message);
    printf("--Review-Complete---------------------------------------------------------------\r\n");
}

void peek_channel(char *channel)
{
    bufptr *search_ptr = server_buffer;

    while (search_ptr->nextbuf != NULL)
    {
        if(strcmp(search_ptr->nextbuf->channel, channel) == 0)
        {
            rewind_buffer(search_ptr->nextbuf->curr, 15);
            return;
        }

        search_ptr = search_ptr->nextbuf;
    }

    printf("<no history available for \'%s\'>\r\n", channel);
    return;
}

void clear_buffer(bufptr *buffer)
{
    bufline *buffer_entry = buffer->curr;

    /* Start at the end */
    while(buffer_entry->next != NULL)
        buffer_entry = buffer_entry->next;

    while(buffer_entry->prev != NULL)
    {
        buffer_entry = buffer_entry->prev;
        free(buffer_entry->message);
        free(buffer_entry->next);
        buffer_entry->next = NULL;
    }

    buffer->prevbuf->nextbuf = buffer->nextbuf;
    buffer->nextbuf->prevbuf = buffer->prevbuf;
    free(buffer->head->message);
    free(buffer->head);
    free(buffer->channel);
    free(buffer);
}

// Unused/For testing:
void print_message_buffer(bufptr *msgbuf)
{
    bufline *buffer = msgbuf->head;

    while (buffer->next != NULL)
    {
        printf("%s\r\n", buffer->message);
        buffer = buffer->next;
    }
    printf("%s\r\n", buffer->message);
}
