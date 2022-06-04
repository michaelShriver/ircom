#include "handlers.h"

/* Run the event handling loop in it's own thread */
void *irc_event_loop(void *sess)
{
    irc_session_t *s;
    s = sess;

    if (irc_run(s))
    {
        fprintf (stderr, "Could not connect or I/O error: %s\r\n", irc_strerror (irc_errno(sess)));
        exit(1);
    }

    return NULL;
}

void event_join (irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
    char nickbuf[128];
    char timebuf[9];
    time_t now = time(&now);
    struct tm *utc = gmtime(&now);

    strftime(timebuf, 9, "%H:%M:%S", utc);
    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    irc_ctx_t * ctx = irc_get_ctx(session);
    bufptr *message_buffer = channel_buffer(params[0]);

    if(strcmp(nickbuf, ctx->nick) == 0)
    {
        buffer_read_ptr = message_buffer->curr;
        ctx->active_channel = message_buffer->channel;
        irc_cmd_user_mode (session, "+i");
    }
    else
        add_member(params[0], nickbuf);

    char joinmsg[290];
    snprintf(joinmsg, 277, "\e[33;1m[%s] %s has joined %s.\e[0m", timebuf, nickbuf, params[0]);
    message_buffer->curr = add_to_buffer(message_buffer, joinmsg);
    message_buffer->curr->isread = strcmp(ctx->active_channel, params[0]) == 0 ? 0 : 1;
    if(strcmp(nickbuf, ctx->nick) == 0)
        message_buffer->curr->isread = 1;

    print_new_messages();
}

void event_quit(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
    char nickbuf[128];
    char timebuf[9];
    time_t now = time(&now);
    struct tm *utc = gmtime(&now);

    strftime(timebuf, 9, "%H:%M:%S", utc);
    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    irc_ctx_t * ctx = irc_get_ctx(session);

    char partmsg[1200];
    bufptr *search_ptr = server_buffer;
    while(search_ptr->nextbuf != NULL)
    {
        if(delete_member(search_ptr->nextbuf->channel, nickbuf))
        {
            snprintf(partmsg, 1200, "\e[33;1m[%s] %s has left %s. (%s)\e[0m", timebuf, nickbuf, search_ptr->nextbuf->channel, params[0]);
            search_ptr->nextbuf->curr = add_to_buffer(search_ptr->nextbuf, partmsg);
        }
        search_ptr = search_ptr->nextbuf;
    }

    print_new_messages();

    return;
}

void event_kick(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
    irc_ctx_t * ctx = irc_get_ctx(session);
    char nickbuf[128];
    char timebuf[9];
    time_t now = time(&now);
    struct tm *utc = gmtime(&now);

    strftime(timebuf, 9, "%H:%M:%S", utc);
    irc_target_get_nick(origin, nickbuf, sizeof(nickbuf));

    if(strcmp(params[1], ctx->nick) == 0)
    {
        bufptr *doomed_buffer = channel_buffer(params[0]);

        char kickmsg[277];
        if(strcmp(params[1], "") == 0)
            snprintf(kickmsg, 277, "[you have been kicked from \'%s\'.]", params[0]);
        else if(strcmp(params[2], "") == 0)
            snprintf(kickmsg, 277, "[you have been kicked from \'%s\' by %s.]", params[0], nickbuf);
        else
        {
            char *reason = irc_color_strip_from_mirc(params[2]);
            snprintf(kickmsg, 277, "[you have been kicked from \'%s\' by %s (%s).]", params[0], nickbuf, reason);
            free(reason);
        }
        
        server_buffer->curr = add_to_buffer(server_buffer, kickmsg);
        server_buffer->curr->isread = 1;
        
        while(input_wait == 1)
            sleep((double).1);
        printf("%s\r\n\r\n", kickmsg);

        if (strcmp(ctx->active_channel, params[0]) == 0)
        {
            if(doomed_buffer->nextbuf != NULL)
            {
                buffer_read_ptr=doomed_buffer->nextbuf->curr;
                ctx->active_channel = doomed_buffer->nextbuf->channel;
            }
            else
            {
                buffer_read_ptr = doomed_buffer->prevbuf->curr;
                ctx->active_channel = doomed_buffer->prevbuf->channel;
            }

            if(channel_buffer(ctx->active_channel) != server_buffer)
                irc_cmd_names(session, ctx->active_channel);
        }

        clear_buffer(doomed_buffer);
    }
    else
    {
        char kickmsg[277];
        bufptr *message_buffer = channel_buffer(params[0]);
        delete_member(params[0], params[1]);
        if(strcmp(params[1], "") == 0)
            snprintf(kickmsg, 277, "\e[33;1m[%s] %s was kicked from \'%s\'.\e[0m", timebuf, params[1], params[0]);
        else if(strcmp(params[2], "") == 0)
            snprintf(kickmsg, 277, "\e[33;1m[%s] %s was kicked from \'%s\' by %s.\e[0m", timebuf, params[1], params[0], nickbuf);
        else
        {
            char *reason = irc_color_strip_from_mirc(params[2]);
            snprintf(kickmsg, 277, "\e[33;1m[%s] %s was kicked from \'%s\' by %s (%s).\e[0m", timebuf, params[1], params[0], nickbuf, reason);
            free(reason);
        }
        message_buffer->curr = add_to_buffer(message_buffer, kickmsg);
        message_buffer->curr->isread = strcmp(ctx->active_channel, params[0]) == 0 ? 0 : 1;
        print_new_messages();
    }

    return;
}

void event_part(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
    irc_ctx_t * ctx = irc_get_ctx(session);
    char nickbuf[128];
    char timebuf[9];
    time_t now = time(&now);
    struct tm *utc = gmtime(&now);

    strftime(timebuf, 9, "%H:%M:%S", utc);
    irc_target_get_nick(origin, nickbuf, sizeof(nickbuf));

    if(strcmp(nickbuf, ctx->nick) == 0)
    {
        bufptr *doomed_buffer = channel_buffer(params[0]);

        if (strcmp(ctx->active_channel, params[0]) == 0)
        {
            if(doomed_buffer->nextbuf != NULL)
            {
                buffer_read_ptr=doomed_buffer->nextbuf->curr;
                ctx->active_channel = doomed_buffer->nextbuf->channel;
            }
            else
            {
                buffer_read_ptr = doomed_buffer->prevbuf->curr;
                ctx->active_channel = doomed_buffer->prevbuf->channel;
            }

            irc_cmd_names(session, ctx->active_channel);
        }
        else
        {
            while(input_wait == 1)
                sleep((double).1);
            if(strcmp(ctx->active_channel, params[0]))
                printf("[you have left \'%s\']\r\n", params[0]);
        }

        clear_buffer(doomed_buffer);
    }
    else
    {
        char partmsg[288];
        bufptr *message_buffer = channel_buffer(params[0]);
        delete_member(params[0], nickbuf);
        snprintf(partmsg, 277, "\e[33;1m[%s] %s has left %s.\e[0m", timebuf, nickbuf, params[0]);
        message_buffer->curr = add_to_buffer(message_buffer, partmsg);
        message_buffer->curr->isread = strcmp(ctx->active_channel, params[0]) == 0 ? 0 : 1;
        print_new_messages();
    }

    return;
}

void event_topic(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
    char nickbuf[128];

    irc_ctx_t *ctx = irc_get_ctx(session);
    irc_target_get_nick(origin, nickbuf, sizeof(nickbuf));
    
    if(channel_isjoined(params[0]))
    {
        char topicmsg[1186];
        char timebuf[9];
        time_t now = time(&now);
        struct tm *utc = gmtime(&now);
        strftime(timebuf, 9, "%H:%M:%S", utc);
        bufptr *message_buffer = channel_buffer(params[0]);
        size_t nicklen = (sizeof(*nickbuf) * (strlen(nickbuf) + 1));
        char *nick = malloc(nicklen);
        char *topic = irc_color_strip_from_mirc(params[1]);
        memcpy(nick, nickbuf, nicklen);

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
        message_buffer->curr = add_to_buffer(message_buffer, topicmsg);

        print_new_messages();
    }

    return;
}

void event_connect(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
    irc_ctx_t *ctx = irc_get_ctx(session);
    if(ctx->active_channel != NULL)
        irc_cmd_join (session, ctx->active_channel, 0);

    return;
}

void event_channel (irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
    irc_ctx_t * ctx = irc_get_ctx(session);
    char nickbuf[128];
    char nick[141];
    char messageline[2048];

    if ( count != 2 )
        return;

    if ( !origin )
        return;

    char *msgbuf = irc_color_strip_from_mirc(params[1]);
    irc_target_get_nick(origin, nickbuf, sizeof(nickbuf));
    bufptr *message_buffer = channel_buffer(params[0]);
    snprintf(nick, 141, "\e[36;1m[%s]\e[0m", nickbuf);
    if (nickwidth_timer() || (strlen(nick) > message_buffer->nickwidth))
    {
        message_buffer->nickwidth = strlen(nick);
        nickwidth_set_at = time(NULL);
    }
    snprintf(messageline, 2048, "%-*s %s", message_buffer->nickwidth, nick, msgbuf);
    message_buffer->curr = add_to_buffer(message_buffer, messageline);
    message_buffer->curr->isread = strcmp(ctx->active_channel, params[0]) == 0 ? 0 : 1;

    print_new_messages();
    return;
}

void event_action(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
    char nickbuf[128];
    char actionbuf[1024];
    char *action = irc_color_strip_from_mirc(params[1]);

    bufptr *message_buffer = channel_buffer(params[0]);

    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    snprintf(actionbuf, 1024, "\e[32;1m<%s %s>\e[0m", nickbuf, action);
    free(action);
    message_buffer->curr = add_to_buffer(message_buffer, actionbuf);

    print_new_messages();
    return;
}

void event_privmsg (irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
    char nickbuf[128];
    char *message = irc_color_strip_from_mirc(params[1]);

    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    while(input_wait == 1)
        sleep((double).1);
    printf ("\e[31;1m*%s*\e[0m %s\r\n", origin ? nickbuf : "someone", message);
    free(message);
}

void event_notice (irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
    char nickbuf[128];
    char *message = irc_color_strip_from_mirc(params[1]);
    char timebuf[9];
    time_t now = time(&now);
    struct tm *utc = gmtime(&now);
    strftime(timebuf, 9, "%H:%M:%S", utc);

    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));
    while(input_wait == 1)
        sleep((double).1);
    printf ("\e[33;1m[%s] NOTICE from %s: %s\e[0m\r\n", timebuf, origin ? nickbuf : "someone", message);
    free(message);
}

void event_numeric (irc_session_t *session, unsigned int event, const char *origin, const char **params, unsigned int count)
{
    switch(event)
    {
        case 1:
        {
            irc_ctx_t *ctx = irc_get_ctx(session);
            memcpy(ctx->nick, params[0], 128);

            char *msgbuf = irc_color_strip_from_mirc(params[count-1]);
            server_buffer->curr = add_to_buffer(server_buffer, msgbuf);
            free(msgbuf);

            print_new_messages();
            break;
        }
        case 321:
        {
            fprintf(pager, "\r\n        channel-name   #  modes/topic\r\n");
            fprintf(pager, "--------------------------------------------------------------------------------\r\n");
            break;
        }
        case 322:
        {
           fprintf(pager, "%20s %3s  %s\r\n", params[1], params[2], params[3]); 
           break;
        }
        case 323:
        {
            output_wait = 0;
            break;
        }
        case 332:
        {
            irc_ctx_t *ctx = irc_get_ctx(session);
    
            char topicmsg[1186];
            char timebuf[9];
            time_t now = time(&now);
            struct tm *utc = gmtime(&now);
            strftime(timebuf, 9, "%H:%M:%S", utc);
            bufptr *message_buffer = channel_buffer(params[1]);
            char *topic = irc_color_strip_from_mirc(params[2]);

            message_buffer->topic = topic;

            snprintf(topicmsg, 1186, "\e[33;1m[%s] TOPIC: %s\e[0m", timebuf, topic);
            message_buffer->curr = add_to_buffer(message_buffer, topicmsg);

            print_new_messages();
            break;
        }
        case 353:
        {
            size_t nicklen = (sizeof(*params[3]) * (strlen(params[3]) + 1));
            char *nicks = malloc(nicklen);
            memcpy(nicks, params[3], nicklen);

            char *nickbuf = strtok(nicks, " ");
            while(nickbuf != NULL)
            {
                add_member(params[2], nickbuf);
                nickbuf = strtok(NULL, " "); 
            }
            free(nicks);

            break;
        }
        case 366:
        {
            irc_ctx_t *ctx = irc_get_ctx(session);

            if(strcmp(params[1], ctx->active_channel) == 0)
            {
                bufptr *active_channel = channel_buffer(ctx->active_channel);
                nickname *search_ptr = active_channel->nicklist;

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
                            size_t nicklen = (sizeof(*search_ptr->handle) * (strlen(search_ptr->handle) + 2));
                            char *nickbuf = malloc(nicklen);
                            if(search_ptr->mode == '\0')
                            {
                                memcpy(nickbuf, search_ptr->handle, nicklen);
                            }
                            else
                            {
                                snprintf(nickbuf, nicklen, "%c%s", search_ptr->mode, search_ptr->handle);
                            }
                            printf("%-20s", nickbuf);
                            search_ptr = search_ptr->next;
                            free(nickbuf);
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
        // Generic events handlers, dump to server buffer:
        case 372:
        {
            // MOTD text
        }
        case 375:
        {
            // Start of MOTD
        }
        case 376:
        {
            // End of MOTD
            char *msgbuf = irc_color_strip_from_mirc(params[count-1]);
            server_buffer->curr = add_to_buffer(server_buffer, msgbuf);
            free(msgbuf);

            print_new_messages();
            break;
        }
        case 403:
        {
            // No such channel
        }
        case 442:
        {
            // Not on channel
        }
        case 461:
        {
            // Need More Parameters
        }
        case 466:
        {
            // Chan ops needed
        }
        case 476:
        {
            // Bad channel mask
        }
        default:
        {
            char event_message[2048];

            snprintf(event_message, 2048, "%d: %s", event, origin);

            for(int i=0; i<count; i++)
            {
                strcat(event_message, " ");
                strcat(event_message, params[i]);
            }

            server_buffer->curr = add_to_buffer(server_buffer, event_message);

            print_new_messages();

            break;
        }
    }
}

/* A placeholder function for events we have not implemented */
void dump_event (irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
    char event_message[2048];
    
    snprintf(event_message, 2048, "%s: %s", event, origin);

    for(int i=0; i<count; i++)
    {
        strcat(event_message, " ");
        strcat(event_message, params[i]);
    }

    server_buffer->curr = add_to_buffer(server_buffer, event_message);

    print_new_messages();
}
