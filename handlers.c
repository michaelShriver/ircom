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
        printf("[you are in \'%s\']\r\n\r\n", ctx->active_channel);
    }

    char joinmsg[277];
    snprintf(joinmsg, 277, "\e[33;1m[%s] %s has joined %s.\e[0m", timebuf, nickbuf, chanbuf);
    message_buffer->curr = add_to_buffer(message_buffer, joinmsg);
    message_buffer->curr->isread = strcmp(ctx->active_channel, chanbuf) == 0 ? 0 : 1;
    if(strcmp(nickbuf, ctx->nick) == 0)
        message_buffer->curr->isread = 1;


    print_new_messages();
}

void event_quit(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    char nickbuf[128];
    char timebuf[9];
    time_t now = time(&now);
    struct tm *utc = gmtime(&now);

    strftime(timebuf, 9, "%H:%M:%S", utc);
    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (session);

    char partmsg[1200];
    snprintf(partmsg, 1200, "\e[33;1m[%s] %s has quit. (%s)\e[0m", timebuf, nickbuf, params[0]);
    server_buffer->curr = add_to_buffer(server_buffer, partmsg);

    print_new_messages();

    return;
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
        }
        else
            printf("[you have been removed from \'%s\']\r\n", chanbuf);

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

void event_topic(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
    char chanbuf[128];
    char nickbuf[128];

    irc_ctx_t *ctx = (irc_ctx_t *)irc_get_ctx(session);
    irc_target_get_nick(origin, nickbuf, sizeof(nickbuf));
    strncpy(chanbuf, params[0], sizeof(chanbuf));
    
    if(channel_isjoined(chanbuf))
    {
        char topicmsg[1186];
        char timebuf[9];
        time_t now = time(&now);
        struct tm *utc = gmtime(&now);
        strftime(timebuf, 9, "%H:%M:%S", utc);
        bufptr *message_buffer = channel_buffer(chanbuf);
        char *nick = malloc((sizeof(char)*strlen(nickbuf))+1);
        char *topic = irc_color_strip_from_mirc(params[1]);
        strcpy(nick, nickbuf);

        if(message_buffer->topic != NULL)
        {
            char *oldtopic = message_buffer->topic;
            message_buffer->topic = topic;
            free(oldtopic);
        }
        else
            message_buffer->topic = topic;

        if(message_buffer->topicsetby != NULL)
        {
            char *oldtopicsetby = message_buffer->topicsetby;
            message_buffer->topicsetby = nick;
            free(oldtopicsetby);
        }
        else
            message_buffer->topicsetby = nick;

        snprintf(topicmsg, 1186, "\e[33;1m[%s] TOPIC: %s (%s)\e[0m", timebuf, topic, nick);
        add_to_buffer(message_buffer, topicmsg);

        print_new_messages();
    }

    return;
}

void event_connect(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    irc_ctx_t *ctx = (irc_ctx_t *)irc_get_ctx(session);
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
    //char msgbuf[1024];
    char messageline[2048];

    if ( count != 2 )
        return;

    if ( !origin )
        return;

    strcpy(chanbuf, params[0]);
    //strcpy(msgbuf, params[1]);
    char *msgbuf = irc_color_strip_from_mirc(params[1]);
    irc_target_get_nick(origin, nickbuf, sizeof(nickbuf));
    bufptr *message_buffer = channel_buffer(chanbuf);
    snprintf(nick, 128, "\e[36;1m[%s]\e[0m", nickbuf);
    message_buffer->nickwidth = (strlen(nick)) > message_buffer->nickwidth ? (strlen(nick)) : message_buffer->nickwidth;

    snprintf(messageline, 2048, "%-*s %s", message_buffer->nickwidth, nick, msgbuf);
    message_buffer->curr = add_to_buffer(message_buffer, messageline);
    message_buffer->curr->isread = strcmp(ctx->active_channel, chanbuf) == 0 ? 0 : 1;

    print_new_messages();
    return;
}

void event_action(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    char nickbuf[128];
    char actionbuf[1024];
    char chanbuf[128];
    char *action = irc_color_strip_from_mirc(params[1]);

    strcpy(chanbuf, params[0]);
    bufptr *message_buffer = channel_buffer(chanbuf);

    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    snprintf(actionbuf, 1024, "\e[32;1m<%s %s>\e[0m", nickbuf, action);
    free(action);
    message_buffer->curr = add_to_buffer(message_buffer, actionbuf);

    print_new_messages();
    return;
}

void event_privmsg (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    char nickbuf[128];
    char *message = irc_color_strip_from_mirc(params[1]);

    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    printf ("\e[31;1m*%s*\e[0m %s\r\n", origin ? nickbuf : "someone", message);
    free(message);
}

void event_numeric (irc_session_t * session, unsigned int event, const char * origin, const char ** params, unsigned int count)
{
    switch(event)
    {
        case 322:
        {
           printf("%13s %3s  %s\r\n", params[1], params[2], params[3]); 
           break;
        }
        case 332:
        {
            char chanbuf[128];

            irc_ctx_t *ctx = (irc_ctx_t *)irc_get_ctx(session);
            strncpy(chanbuf, params[1], sizeof(chanbuf));
    
            char topicmsg[1186];
            char timebuf[9];
            time_t now = time(&now);
            struct tm *utc = gmtime(&now);
            strftime(timebuf, 9, "%H:%M:%S", utc);
            bufptr *message_buffer = channel_buffer(chanbuf);
            char *topic = irc_color_strip_from_mirc(params[2]);

            message_buffer->topic = topic;

            snprintf(topicmsg, 1186, "\e[33;1m[%s] TOPIC: %s\e[0m", timebuf, topic);
            add_to_buffer(message_buffer, topicmsg);

            print_new_messages();
            printf("\r\n");
            break;
        }
        case 353:
        {
            //printf("%d %s %u %s %s\r\n", event, origin, count, params[2], params[3]);
            irc_ctx_t *ctx = (irc_ctx_t *)irc_get_ctx(session);
            if(strcmp(ctx->active_channel, params[2]) == 0)
            {
                int cols = (ttysize.ws_col / 20);
                int nicksize = strlen(params[3]) + 1;
                char nicks[nicksize];
                strncpy(nicks, params[3], nicksize);
                char *nickbuf = strtok(nicks, " ");

                while(nickbuf != NULL)
                {
                    for(int c=0; c < cols; c++)
                    {
                        if(nickbuf != NULL)
                        {
                            printf("%-20s", nickbuf);
                            nickbuf = strtok(NULL, " ");
                        }
                        else
                            break;
                    }
                    printf("\r\n");
                }
                break;
            }

            // break;
        }
        case 372:
        case 375:
        case 376:
        {
            char *msgbuf = irc_color_strip_from_mirc(params[1]);
            add_to_buffer(server_buffer, msgbuf);
            free(msgbuf);

            print_new_messages();
            break;
        }
        //ignore events
        case 366:
        {
            break;
        }
        default:
        {
            char buf[24];
            sprintf (buf, "%d", event);
            dump_event (session, buf, origin, params, count);
        }
    }
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
