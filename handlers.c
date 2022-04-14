#include "handlers.h"

/* Run the event handling loop in it's own thread */
void *irc_event_loop(void * sess)
{
    irc_session_t * s;
    s = sess;

    if (irc_run(s))
    {
        printf ("Could not connect or I/O error: %s", irc_strerror (irc_errno(sess)));
        exit(1);
    }

    return NULL;
}

void event_join (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    char nickbuf[128];
    char chanbuf[128];
    char timebuf[9];
    time_t now = time(&now);
    struct tm *utc = gmtime(&now);

    strftime(timebuf, 9, "%H:%M:%S", utc);
    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    strcpy(chanbuf, params[0]);
    irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (session);
    bufptr *message_buffer = channel_buffer(chanbuf);

    if(strcmp(nickbuf, ctx->nick) == 0)
    {
        buffer_read_ptr = message_buffer->curr; // Move read pointer to first channel buffer upon join
        strcpy(ctx->active_channel, chanbuf);
        irc_cmd_user_mode (session, "+i");
    }

    char joinmsg[277];
    snprintf(joinmsg, 277, "\e[33;1m[%s] %s has joined %s.\e[0m", timebuf, nickbuf, chanbuf);
    message_buffer->curr = add_to_buffer(message_buffer, joinmsg);
    message_buffer->curr->isread = strcmp(ctx->active_channel, chanbuf) == 0 ? 0 : 1;

    print_new_messages();
}

void event_part(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    char nickbuf[128];
    char chanbuf[128];
    char timebuf[9];
    time_t now = time(&now);
    struct tm *utc = gmtime(&now);

    strftime(timebuf, 9, "%H:%M:%S", utc);
    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    strcpy(chanbuf, params[0]);
    irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (session);

    if(strcmp(nickbuf, ctx->nick) == 0)
    {
        bufptr *doomed_buffer = channel_buffer(chanbuf);

        if (strcmp(ctx->active_channel, chanbuf) == 0)
        {
            buffer_read_ptr = doomed_buffer->prevbuf->curr;
            strcpy(ctx->active_channel, doomed_buffer->prevbuf->channel);
            printf("<now chatting in \'%s\'>\r\n", ctx->active_channel);
        }
        else
            printf("<you have been removed from \'%s\'>\r\n", chanbuf);

        clear_buffer(doomed_buffer);
    }
    else
    {
        char partmsg[277];
        bufptr *message_buffer = channel_buffer(chanbuf);
        snprintf(partmsg, 277, "\e[33;1m[%s] %s has left %s.\e[0m", timebuf, nickbuf, chanbuf);
        message_buffer->curr = add_to_buffer(message_buffer, partmsg);
        message_buffer->curr->isread = strcmp(ctx->active_channel, chanbuf) == 0 ? 0 : 1;
    }

    print_new_messages();

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
    char nick[128];
    char chanbuf[128];
    char msgbuf[1024];
    char messageline[1156];

    if ( count != 2 )
        return;

    if ( !origin )
        return;

    strcpy(chanbuf, params[0]);
    strcpy(msgbuf, params[1]);
    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    bufptr *message_buffer = channel_buffer(chanbuf);
    snprintf(nick, 128, "\e[36;1m[%s]\e[0m", nickbuf);
    message_buffer->nickwidth = (strlen(nick)) > message_buffer->nickwidth ? (strlen(nick)) : message_buffer->nickwidth;

    snprintf(messageline, 1156, "%-*s %s", message_buffer->nickwidth, nick, msgbuf);
    message_buffer->curr = add_to_buffer(message_buffer, messageline);
    message_buffer->curr->isread = strcmp(ctx->active_channel, chanbuf) == 0 ? 0 : 1;

    print_new_messages();

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
    snprintf(actionbuf, 256, "\e[32;1m<%s %s>\e[0m", nickbuf, params[1]);
    message_buffer->curr = add_to_buffer(message_buffer, actionbuf);

    print_new_messages();
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
