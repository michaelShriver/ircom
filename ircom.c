#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include "pthread.h"
#include "libircclient.h"

/* Data structures */

typedef struct
{
    char *channel;
    char *nick;

} irc_ctx_t;

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
    bufline *head;
    bufline *curr;
};

struct termios termstate;
bufptr *message_buffer;

/* Functions */

void dump_event();
void event_numeric();
void event_join();
void event_connect();
void event_privmsg();
void event_channel();
void dcc_recv_callback();
void dcc_file_recv_callback();
void addlog(const char *, ...);

void *irc_event_loop(void *);
bufline *add_to_buffer();
int kbhit();
char *get_input();
void send_message();
void show_prompt();
void reset_termstate();
void rewind_buffer();
void print_message_buffer();

int main(int argc, char **argv)
{
    if ( argc != 4 )
    {
        printf ("Usage: %s <server> <nick> <channel>\n", argv[0]);
        return 1;
    }

    /* Initialize callbacks and pointers */
    irc_callbacks_t callbacks;
    irc_ctx_t ctx;
    irc_session_t * sess;
    unsigned short port = 6667;
    char *input = NULL;
    pthread_t event_thread;
    char keycmd;

    /* Save terminal state, and restore on exit */
    struct termios termstate_raw;
    tcgetattr(0, &termstate);
    memcpy(&termstate_raw, &termstate, sizeof(termstate_raw)); 
    atexit(reset_termstate);

    /* Initialize the message buffer and create the first entry */
    message_buffer = malloc(sizeof(struct bufptr));
    bufline *newbuf = (struct bufline*) malloc(sizeof(struct bufline));
    message_buffer->head = newbuf;
    message_buffer->curr = newbuf;
    message_buffer->curr->prev = NULL;
    message_buffer->curr->next = NULL;
    message_buffer->curr->message = NULL;
    message_buffer->curr->isread = NULL;

    bufline *buffer_read_ptr = newbuf;

    /* Zero out memory allocation for callbacks struct */
    memset (&callbacks, 0, sizeof(callbacks));

    callbacks.event_connect = event_connect;
    callbacks.event_join = event_join;
    callbacks.event_nick = dump_event;
    callbacks.event_quit = dump_event;
    callbacks.event_part = dump_event;
    callbacks.event_mode = dump_event;
    callbacks.event_topic = dump_event;
    callbacks.event_kick = dump_event;
    callbacks.event_channel = event_channel;
    callbacks.event_privmsg = event_privmsg;
    callbacks.event_notice = dump_event;
    callbacks.event_invite = dump_event;
    callbacks.event_umode = dump_event;
    callbacks.event_ctcp_rep = dump_event;
    callbacks.event_ctcp_action = dump_event;
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

    ctx.channel = argv[3];
    ctx.nick = argv[2];

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
        //input = get_input();
        //irc_cmd_msg(sess, ctx.channel, input);
        //free(input);

        /* Walk the message buffer, and write messages to the terminal */
        while (buffer_read_ptr->next != NULL)
        {
            if (buffer_read_ptr->isread == 0)
            {
                buffer_read_ptr->isread = 1;
                printf("%s\r\n", buffer_read_ptr->message);
            }

            if (buffer_read_ptr->next != NULL)
            {
                buffer_read_ptr = buffer_read_ptr->next;
            }
        }

        if (buffer_read_ptr->isread == 0)
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

            if (stroke == 'q')
            {
                return 0;
            }
            else if (stroke == 'r')
            {
                rewind_buffer(buffer_read_ptr);
            }
            else if (stroke == 'R')
            {
                print_message_buffer(message_buffer);
            }
            else
            {
                tcsetattr(0, TCSANOW, &termstate);

                char *input;
                show_prompt(ctx);
                input = get_input();
                send_message(sess, ctx, input);
                free(input);

                tcsetattr(0, TCSANOW, &termstate_raw);
            }
        }
    }

    free(newbuf);
	return 0;
}

/* Add a message to the end of the message buffer, and update pointers */
bufline *add_to_buffer(bufline *msgbuffer, char *cmsg)
{
    char *messageline = (char*)malloc(sizeof(char) * (strlen(cmsg) + 1));
    strcpy(messageline, cmsg);

    /* Ensure we are at the end of the buffer */
    while (msgbuffer->next != NULL)
    {
        msgbuffer = msgbuffer->next;
    }

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
void send_message(irc_session_t *s, irc_ctx_t ctx, char *message)
{
    int size = sizeof(char) * (strlen(ctx.nick) + strlen(message) + 3);
    char *bufferline = (char *)malloc(size);

    snprintf(bufferline, size, "[%s] %s", ctx.nick, message);
    irc_cmd_msg(s, ctx.channel, message);
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

    for ( cnt = 0; cnt < count; cnt++ )
    {
        if ( cnt )
            strcat (buf, "|");

        strcat (buf, params[cnt]);
    }

    addlog ("Event \"%s\", origin: \"%s\", params: %d [%s]", event, origin ? origin : "NULL", cnt, buf);
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

void addlog (const char * fmt, ...)
{
    FILE * fp;
    char buf[1024];
    va_list va_alist;

    va_start (va_alist, fmt);
    vsnprintf (buf, sizeof(buf), fmt, va_alist);
    va_end (va_alist);

    message_buffer->curr = add_to_buffer(message_buffer->curr, buf);

    if ( (fp = fopen ("irctest.log", "ab")) != 0 )
    {
        fprintf (fp, "%s\n", buf);
        fclose (fp);
    }
}

void event_join (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    dump_event (session, event, origin, params, count);
    irc_cmd_user_mode (session, "+i");
}

void event_connect (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (session);
    dump_event (session, event, origin, params, count);

    irc_cmd_join (session, ctx->channel, 0);
}

void event_channel (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    char nickbuf[128];
    char chanbuf[128];
    char msgbuf[1024];
    char messageline[1024];

    if ( count != 2 )
        return;

    if ( !origin )
        return;

    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    strcpy(chanbuf, params[0]);
    strcpy(msgbuf, params[1]);

    snprintf(messageline, sizeof(msgbuf), "[%s] %s", nickbuf, msgbuf);
    message_buffer->curr = add_to_buffer(message_buffer->curr, messageline);

    /*
    if ( !strcmp (params[1], "quit") )
        irc_cmd_quit (session, "of course, Master!");

    if ( !strcmp (params[1], "help") )
    {
        irc_cmd_msg (session, params[0], "quit, help, dcc chat, dcc send, ctcp");
    }

    if ( !strcmp (params[1], "ctcp") )
    {
        irc_cmd_ctcp_request (session, nickbuf, "PING 223");
        irc_cmd_ctcp_request (session, nickbuf, "FINGER");
        irc_cmd_ctcp_request (session, nickbuf, "VERSION");
        irc_cmd_ctcp_request (session, nickbuf, "TIME");
    }

    if ( !strcmp (params[1], "dcc chat") )
    {
        irc_dcc_t dccid;
        irc_dcc_chat (session, 0, nickbuf, dcc_recv_callback, &dccid);
        printf ("DCC chat ID: %d\n", dccid);
    }

    if ( !strcmp (params[1], "dcc send") )
    {
        irc_dcc_t dccid;
        irc_dcc_sendfile (session, 0, nickbuf, "irctest.c", dcc_file_recv_callback, &dccid);
        printf ("DCC send ID: %d\n", dccid);
    }

    if ( !strcmp (params[1], "topic") )
        irc_cmd_topic (session, params[0], 0);
    else if ( strstr (params[1], "topic ") == params[1] )
        irc_cmd_topic (session, params[0], params[1] + 6);

    if ( strstr (params[1], "mode ") == params[1] )
        irc_cmd_channel_mode (session, params[0], params[1] + 5);

    if ( strstr (params[1], "nick ") == params[1] )
        irc_cmd_nick (session, params[1] + 5);

    if ( strstr (params[1], "whois ") == params[1] )
        irc_cmd_whois (session, params[1] + 5);
    */
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
    free(message_buffer);
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
    ssize_t read_size;

    read_size = getline(&input, &buffer_size, stdin);

    return input;
}

void rewind_buffer(bufline *buffer_read_ptr)
{
    while (buffer_read_ptr->prev != NULL)
        buffer_read_ptr = buffer_read_ptr->prev;

    while (buffer_read_ptr->next != NULL)
    {
        printf("%s\r\n", buffer_read_ptr->message);
        buffer_read_ptr = buffer_read_ptr->next;
    }
    printf("%s\r\n", buffer_read_ptr->message);
}

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
