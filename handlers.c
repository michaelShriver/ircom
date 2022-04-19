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
        buffer_read_ptr = message_buffer->curr;
        strcpy(ctx->active_channel, chanbuf);
        irc_cmd_user_mode (session, "+i");
        //while(input_wait == 1)
            //sleep((double).1);
        //printf("[you are in \'%s\']\r\n\r\n", ctx->active_channel);
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
    bufptr *search_ptr = server_buffer;
    while(search_ptr->nextbuf != NULL)
    {
        if(delete_member(search_ptr->nextbuf->channel, nickbuf))
        {
            snprintf(partmsg, 1200, "\e[33;1m[%s] %s has left %s. (Quit: %s)\e[0m", timebuf, nickbuf, search_ptr->nextbuf->channel, params[0]);
            search_ptr->nextbuf->curr = add_to_buffer(search_ptr->nextbuf, partmsg);
        }
        search_ptr = search_ptr->nextbuf;
    }
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
            if(doomed_buffer->nextbuf != NULL)
            {
                buffer_read_ptr=doomed_buffer->nextbuf->curr;
                strcpy(ctx->active_channel, doomed_buffer->nextbuf->channel);
            }
            else
            {
                buffer_read_ptr = doomed_buffer->prevbuf->curr;
                strcpy(ctx->active_channel, doomed_buffer->prevbuf->channel);
            }
            irc_cmd_names(session, ctx->active_channel);
        }
        else
            while(input_wait == 1)
                sleep((double).1);
            if(strcmp(ctx->active_channel, chanbuf))
                printf("[you have been removed from \'%s\']\r\n", chanbuf);

        clear_buffer(doomed_buffer);
    }
    else
    {
        char partmsg[277];
        bufptr *message_buffer = channel_buffer(chanbuf);
        delete_member(chanbuf, nickbuf);
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
    while(input_wait == 1)
        sleep((double).1);
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
            break;
        }
        case 353:
        {
            char nicks[strlen(params[3]+1)];
            strcpy(nicks, params[3]);

            char *nickbuf = strtok(nicks, " ");
            while(nickbuf != NULL)
            {
                add_member(params[2], nickbuf);
                nickbuf = strtok(NULL, " "); 
            }

            break;
        }
        case 366:
        {
            irc_ctx_t *ctx = (irc_ctx_t *)irc_get_ctx(session);

            if(strcmp(params[1], ctx->active_channel) == 0)
            {
                bufptr *active_channel = channel_buffer(ctx->active_channel);
                nickname *search_ptr = active_channel->nicklist;
                char nickbuf[128];

                while(input_wait == 1)
                    sleep((double).1);
                printf("\r\n[you are in \'%s\' among %d]\r\n\r\n", ctx->active_channel, active_channel->nickcount);

                int cols = (ttysize.ws_col / 20);
                while(search_ptr != NULL)
                {
                    for(int c=0; c < cols; c++)
                    {
                        if(search_ptr != NULL)
                        {
                            if(search_ptr->mode == '\0')
                            {
                                strncpy(nickbuf, search_ptr->handle, sizeof(nickbuf));
                            }
                            else
                            {
                                snprintf(nickbuf, sizeof(nickbuf), "%c%s", search_ptr->mode, search_ptr->handle);
                            }
                            printf("%-20s", nickbuf);
                            search_ptr = search_ptr->next;
                        }
                        else
                            break;
                    }
                    printf("\r\n");
                }
            }
            printf("\r\n");
            break;
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
        //case 366:
        //{
        //    break;
        //}
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
