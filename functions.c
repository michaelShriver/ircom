#include "functions.h"

bufptr *init_buffer(irc_session_t *s, const char *channel)
{
    irc_ctx_t *ctx = irc_get_ctx(s);
    size_t chanlen = (sizeof(*channel) * (strlen(channel) + 1));
    char *bufchan = malloc(chanlen);
    memcpy(bufchan, channel, chanlen);

    bufptr *channel_buffer = malloc(sizeof(struct bufptr));
    channel_buffer->head = channel_buffer->curr = malloc(sizeof(struct bufline));
    channel_buffer->channel = bufchan;
    channel_buffer->topic = NULL;
    channel_buffer->topicsetby = NULL;
    channel_buffer->nickwidth = 11;
    channel_buffer->nickcount = 0;
    channel_buffer->nicklist = NULL;
    channel_buffer->nickwidth_set_at = time(NULL);
    channel_buffer->curr->prev = NULL;
    channel_buffer->curr->next = NULL;
    channel_buffer->curr->message = NULL;
    channel_buffer->curr->isread = NULL;
    channel_buffer->prevbuf = NULL;
    channel_buffer->nextbuf = NULL;
    if (ctx->server_buffer != NULL)
        ctx->server_buffer->prevbuf = channel_buffer;

    return channel_buffer;
}

bufptr *init_server_buffer(irc_session_t *s, char *server)
{
    irc_ctx_t *ctx = irc_get_ctx(s);
    size_t chanlen = (sizeof(*server) * (strlen(server) + 1));
    char *bufchan = malloc(chanlen);
    memcpy(bufchan, server, chanlen);

    bufptr *channel_buffer = malloc(sizeof(struct bufptr));
    channel_buffer->head = channel_buffer->curr = malloc(sizeof(struct bufline));
    channel_buffer->channel = bufchan;
    channel_buffer->topic = NULL;
    channel_buffer->topicsetby = NULL;
    channel_buffer->nickwidth = 11;
    channel_buffer->nickcount = 0;
    channel_buffer->nicklist = NULL;
    channel_buffer->curr->prev = NULL;
    channel_buffer->curr->next = NULL;
    channel_buffer->curr->message = NULL;
    channel_buffer->curr->isread = NULL;
    channel_buffer->prevbuf = NULL;
    channel_buffer->nextbuf = NULL;

    return channel_buffer;
}

nickname *init_nickentry()
{
    nickname *nickentry = malloc(sizeof(nickname));
    nickentry->mode = '\0';
    nickentry->handle = NULL;
    nickentry->next = NULL;

    return nickentry;
}

/* Return a pointer for a specified channel's buffer, init a new buffer if needed */
bufptr *channel_buffer(irc_session_t *s, const char *channel)
{
    irc_ctx_t *ctx = irc_get_ctx(s);
    bufptr *search_ptr = ctx->server_buffer;

    if(strcmp(search_ptr->channel, channel) == 0)
        return search_ptr;

    while (search_ptr->nextbuf != NULL)
    {
        if(strcmp(search_ptr->nextbuf->channel, channel) == 0)
            return search_ptr->nextbuf;

        search_ptr = search_ptr->nextbuf;
    }

    bufptr *newbuf = init_buffer(s, channel);
    newbuf->prevbuf = search_ptr;
    search_ptr->nextbuf = newbuf;

    return search_ptr->nextbuf;
}

int channel_isjoined(irc_session_t *s, const char *channel)
{
    irc_ctx_t * ctx = irc_get_ctx(s);
    bufptr *search_ptr = ctx->server_buffer;

    while (search_ptr->nextbuf != NULL)
    {
        if(strcmp(search_ptr->nextbuf->channel, channel) == 0)
            return 1;

        search_ptr = search_ptr->nextbuf;
    }

    return 0;
}

int nick_is_member(irc_session_t *s, char *channel, char *nick)
{
    nickname *search_ptr = channel_buffer(s, channel)->nicklist;
    char modeset[] = "~&@%+";

    if(search_ptr == NULL)
        return 0;

    for(int i=0; i < strlen(modeset); i++)
    {
        if(nick[0] == modeset[i])
        {
            nick++;
            break;
       }
    }

    if(strcmp(nick, search_ptr->handle) == 0)
        return 1;

    while(search_ptr->next != NULL)
    {
        if(strcmp(nick, search_ptr->next->handle) == 0)
            return 1;

        search_ptr = search_ptr->next;
    }
    return 0;
}

int add_member(irc_session_t *s, const char *channel, const char *nick)
{
    irc_ctx_t * ctx = irc_get_ctx(s);
    if(!channel_isjoined(s, channel))
    {
        char errmsg[256];
        snprintf(errmsg, 256, "<cannot add %s to nonexistant channel \'%s\'.>", nick, channel);
        ctx->server_buffer->curr = add_to_buffer(ctx->server_buffer, errmsg);

        print_new_messages(s);
        return -1;
    }

    bufptr *chanbuf = channel_buffer(s, channel);
    nickname *search_ptr = chanbuf->nicklist;
    size_t nicklen = (sizeof(char) * (strlen(nick) + 1));
    char mode = '\0';
    char modeset[] = "~&@%+";

    for(int i=0; i < strlen(modeset); i++)
    {
        if(nick[0] == modeset[i] && strlen(nick) > 1)
        {
            mode = nick[0];
            nick++;
            break;
       }
    }

    if(search_ptr == NULL)
    {
        chanbuf->nicklist = init_nickentry();
        chanbuf->nicklist->mode = mode;
        chanbuf->nicklist->handle = malloc(nicklen);
        memcpy(chanbuf->nicklist->handle, nick, nicklen);
        chanbuf->nickcount++;

        return 1;
    }

    if(strcmp(nick, search_ptr->handle) == 0)
    {
        search_ptr->mode = mode;
        return 0;
    }

    while(search_ptr->next != NULL)
    {
        if(strcmp(nick, search_ptr->next->handle) == 0)
        {
            search_ptr->next->mode = mode;
            return 0;
        }

        search_ptr = search_ptr->next;
    }

    search_ptr->next = init_nickentry();
    search_ptr->next->mode = mode;
    search_ptr->next->handle = malloc(nicklen);
    memcpy(search_ptr->next->handle, nick, nicklen);
    chanbuf->nickcount++;

    return 1;
}

int delete_member(irc_session_t *s, const char *channel, const char *nick)
{
    irc_ctx_t * ctx = irc_get_ctx(s);
    if(!channel_isjoined(s, channel))
    {
        char errmsg[256];
        snprintf(errmsg, 256, "<cannot remove %s from nonexistant channel \'%s\'.>", nick, channel);
        ctx->server_buffer->curr = add_to_buffer(ctx->server_buffer, errmsg);

        print_new_messages(s);
        return -1;
    }

    bufptr *chanbuf = channel_buffer(s, channel);
    nickname *search_ptr = chanbuf->nicklist;
    char mode = '\0';
    char modeset[] = "~&@%+";

    for(int i=0; i < strlen(modeset); i++)
    {
        if(nick[0] == modeset[i])
        {
            mode = nick[0];
            nick++;
            break;
       }
    }

    if(search_ptr == NULL)
        return 0;

    if(strcmp(nick, search_ptr->handle) == 0)
    {
        chanbuf->nicklist = search_ptr->next;
        chanbuf->nickcount--;
        free(search_ptr->handle);
        free(search_ptr);

        return 1;
    }

    while(search_ptr->next != NULL)
    {
        if(strcmp(nick, search_ptr->next->handle) == 0)
        {
            nickname *deadmember = search_ptr->next;
            search_ptr->next = deadmember->next;
            chanbuf->nickcount--;
            free(deadmember->handle);
            free(deadmember);
            return 1;
        }

        search_ptr = search_ptr->next;
    }

    return 0;
}

int nickwidth_timer(irc_session_t *s, char *channel)
{
    irc_ctx_t * ctx = irc_get_ctx(s);
    time_t current_time = time(NULL);

    if (current_time > (channel_buffer(s, channel)->nickwidth_set_at + 900))
    {
        return 1;
    }

    return 0;
}

/* Add a message to the end of the message buffer, return a pointer to new entry */
bufline *add_to_buffer(bufptr *msgbuffer, char *cmsg)
{
    size_t msglen = (sizeof(*cmsg) * (strlen(cmsg) + 1)); 
    char *messageline = malloc(msglen);
    memcpy(messageline, cmsg, msglen);

    /* Ensure we are at the end of the buffer */
    while (msgbuffer->curr->next != NULL)
        msgbuffer->curr = msgbuffer->curr->next;

    /* Handle the first node in the message buffer */
    if (msgbuffer->curr->message == NULL)
    {
        msgbuffer->curr->prev = NULL;
        msgbuffer->curr->next = NULL;
        msgbuffer->curr->message = messageline;
        msgbuffer->curr->isread = 0;

        return msgbuffer->curr;
    }

    /* Add a new message to the end of the buffer */
    bufline *newmsg = malloc(sizeof(struct bufline));
    msgbuffer->curr->next = newmsg;
    newmsg->prev = msgbuffer->curr;
    newmsg->next = NULL;
    newmsg->message = messageline;
    newmsg->isread = 0;

    return newmsg;
}

/* Send a message to the channel, and add it to my buffer */
void send_message(irc_session_t *s)
{
    irc_ctx_t * ctx = irc_get_ctx(s);
    char *input;
    char nickbuf[141];
    bufptr *message_buffer = channel_buffer(s, ctx->active_channel);
    int nickwidth_tmp = message_buffer->nickwidth;
    if(message_buffer == ctx->server_buffer)
    {
        printf("<press 'g' to go to channel>\n");       
        return;
    }
    snprintf(nickbuf, 141, "\e[36;1m[%s]\e[0m", ctx->nick);
    if (nickwidth_timer(s, ctx->active_channel) || strlen(nickbuf) > message_buffer->nickwidth)
    {
        message_buffer->nickwidth = strlen(nickbuf);
        message_buffer->nickwidth_set_at = time(NULL);
    }

    printf("%-*s ", message_buffer->nickwidth, nickbuf);
    input = get_input();
    if (strcmp(input, "") == 0)
    {
        free(input);
        message_buffer->nickwidth = nickwidth_tmp;
        printf("<no message sent>\n");
        return;
    }


    int size = sizeof(char) * (strlen(ctx->nick) + strlen(input) + message_buffer->nickwidth);
    char *bufferline = malloc(size);

    snprintf(bufferline, size, "%-*s %s", message_buffer->nickwidth, nickbuf, input);
    irc_cmd_msg(s, ctx->active_channel, input);
    message_buffer->curr = add_to_buffer(message_buffer, bufferline);
    message_buffer->curr->isread = 1;
    free(input);
    free(bufferline);
}

/* Send an action to the channel, and add it to my buffer */
void send_action(irc_session_t *s)
{
    char *action;
    irc_ctx_t *ctx = irc_get_ctx(s);
    bufptr *message_buffer = channel_buffer(s, ctx->active_channel);

    printf("%-*s ", message_buffer->nickwidth-11, ":emote>");
    action = get_input();
    if(strcmp(action, "") == 0)
    {
        printf("<no message sent>\n");
        return;
    }

    int size = sizeof(char) * (strlen(ctx->nick) + strlen(action) + 15);
    char *bufferline = malloc(size);

    snprintf(bufferline, size, "\e[32;1m<%s %s>\e[0m", ctx->nick, action);
    irc_cmd_me(s, ctx->active_channel, action);
    message_buffer->curr = add_to_buffer(message_buffer, bufferline);
    message_buffer->curr->isread = 1;
    free(bufferline);
    free(action);
}

void send_privmsg(irc_session_t *s)
{
    irc_ctx_t *ctx = irc_get_ctx(s);
    char *message;
    char *recipient;
    bufptr *active_channel = channel_buffer(s, ctx->active_channel);
    
    printf("Provide a RECIPIENT\n");
    printf("%-*s ", active_channel->nickwidth-11, ":to>");
    recipient = get_input();
    printf("%-*s ", active_channel->nickwidth-11, ":msg>");
    message = get_input();

    if (!irc_cmd_msg(s, recipient, message))
        printf("\e[32;1m<message sent to %s>\e[0m\n", recipient);

    free(message);
    free(recipient);
    
    return;
}

void change_nick(irc_session_t *s)
{
    irc_ctx_t *ctx = irc_get_ctx(s);
    char *nick;
    bufptr *active_channel = channel_buffer(s, ctx->active_channel);

    printf("%-*s ", active_channel->nickwidth-11, ":nick>");
    nick = get_input();

    irc_cmd_nick(s, nick);

    free(nick);
}

void show_prompt(irc_session_t *s)
{
    irc_ctx_t *ctx = irc_get_ctx(s);

    char nickbuf[130];
    snprintf(nickbuf, 130, "[%s]", ctx->nick);
    printf("%-*s ", (int)strlen(nickbuf)+2, nickbuf);
}

void kick_user(irc_session_t *s)
{
    irc_ctx_t *ctx = irc_get_ctx(s);
    bufptr *active_channel = channel_buffer(s, ctx->active_channel);

    printf("%-*s ", active_channel->nickwidth-11, ":kick>");
    char *user = get_input();
    printf("%-*s ", active_channel->nickwidth-11, ":reason>");
    char *reason = get_input();

    if (strcmp(user, "") == 0)
    {
        irc_cmd_names(s, ctx->active_channel);
    }
    else
    {
        irc_cmd_kick(s, user, ctx->active_channel, reason);
    }

    free(user);
    free(reason);
    return;
}

char * get_input()
{
    char *input = NULL;
    size_t buffer_size = 0;

    getline(&input, &buffer_size, stdin);
    input[strlen(input)-1] = '\0';

    return input;
}

void rewind_buffer(irc_session_t *s, int lines)
{
    irc_ctx_t * ctx = irc_get_ctx(s);
    ioctl(0, TIOCGWINSZ, &ctx->ttysize);
                                   
    if (lines <= 0)
    {
        lines = 0;
        while(lines < (ctx->ttysize.ws_row - 4))
        {
            if (ctx->buffer_read_ptr->prev == NULL)
                break; 
            lines = lines + (((strlen(ctx->buffer_read_ptr->message)) / ctx->ttysize.ws_col) + 1);
            ctx->buffer_read_ptr = ctx->buffer_read_ptr->prev;
        }
        if (ctx->buffer_read_ptr->next != NULL)
            ctx->buffer_read_ptr = ctx->buffer_read_ptr->next;

        printf("--Beginning-Review--------------------------------------------------------------\r\n");
        while (ctx->buffer_read_ptr->next != NULL)
        {
            printf("%s\r\n", ctx->buffer_read_ptr->message);
            ctx->buffer_read_ptr = ctx->buffer_read_ptr->next;
        }
        printf("%s\r\n", ctx->buffer_read_ptr->message);
        printf("--Review-Complete---------------------------------------------------------------\r\n");

    }
    else
    {
        for (lines--; lines > 0; lines--)
        {
            if (ctx->buffer_read_ptr->prev == NULL)
               break; 
            ctx->buffer_read_ptr = ctx->buffer_read_ptr->prev;
        }

        printf("--Beginning-Review--------------------------------------------------------------\r\n");

        int fpstatus;
            
        ctx->pager = popen("less -R", "w");
        if (ctx->pager == NULL)
        {
            fprintf(stderr, "Error opening pager\r\n");
        }

        while (ctx->buffer_read_ptr->next != NULL)
        {
            fprintf(ctx->pager, "%s\r\n", ctx->buffer_read_ptr->message);
            ctx->buffer_read_ptr = ctx->buffer_read_ptr->next;
        }
        fprintf(ctx->pager, "%s\r\n", ctx->buffer_read_ptr->message);

        fpstatus = pclose(ctx->pager);
        if (fpstatus == -1)
        {
            fprintf(stderr, "Pipe returned an error\r\n");
        }

        printf("--Review-Complete---------------------------------------------------------------\r\n");
    }

}

void print_new_messages(irc_session_t *s)
{
    irc_ctx_t *ctx = irc_get_ctx(s);
    if (ctx->input_wait == 0)
    {
        /* Walk the message buffer, and write messages to the terminal */
        while (ctx->buffer_read_ptr->next != NULL)
        {
            if (ctx->buffer_read_ptr->isread == 0 && ctx->buffer_read_ptr->message != NULL)
            {
                ctx->buffer_read_ptr->isread = 1;
                printf("%s\r\n", ctx->buffer_read_ptr->message);
            }
    
            if (ctx->buffer_read_ptr->next != NULL)
            {
                ctx->buffer_read_ptr = ctx->buffer_read_ptr->next;
            }
        }

        if (ctx->buffer_read_ptr->isread == 0 && ctx->buffer_read_ptr->message != NULL)
        {
            ctx->buffer_read_ptr->isread = 1;
            printf("%s\r\n", ctx->buffer_read_ptr->message);
        }
    }
}

void peek_channel(irc_session_t *s)
{
    irc_ctx_t *ctx = irc_get_ctx(s);
    bufptr *search_ptr = ctx->server_buffer;
    char *channel;
    bufptr *active_channel = channel_buffer(s, ctx->active_channel);

    printf("%-*s ", active_channel->nickwidth-11, ":peek>");
    channel = get_input();

    while (search_ptr->nextbuf != NULL)
    {
        if(strcmp(search_ptr->nextbuf->channel, channel) == 0)
        {
            int lines = 0;
            while(lines < (ctx->ttysize.ws_row - 4))
            {
                if (search_ptr->nextbuf->curr->prev == NULL)
                    break; 
                lines = lines + (((strlen(search_ptr->nextbuf->curr->message)) / ctx->ttysize.ws_col) + 1);
                search_ptr->nextbuf->curr = search_ptr->nextbuf->curr->prev;
            }
            if (search_ptr->nextbuf->curr->next != NULL)
                search_ptr->nextbuf->curr = search_ptr->nextbuf->curr->next;

            printf("--Beginning-Review--------------------------------------------------------------\r\n");
            while (search_ptr->nextbuf->curr->next != NULL)
            {
                printf("%s\r\n", search_ptr->nextbuf->curr->message);
                search_ptr->nextbuf->curr = search_ptr->nextbuf->curr->next;
            }
            printf("%s\r\n", search_ptr->nextbuf->curr->message);
            printf("--Review-Complete---------------------------------------------------------------\r\n");
            return;
        }

        search_ptr = search_ptr->nextbuf;
    }

    printf("<no history available for \'%s\'>\r\n", channel);
    free(channel);

    return;
}

void invite_user(irc_session_t *s)
{
    irc_ctx_t * ctx = irc_get_ctx(s);
    char *nickbuf;
    bufptr *active_channel = channel_buffer(s, ctx->active_channel);

    if(active_channel == ctx->server_buffer)
    {
        printf("<you must be in a channel\r\n");
        return;
    }

    printf("%-*s ", active_channel->nickwidth-11, ":user>");
    nickbuf = get_input();
    irc_cmd_invite(s, nickbuf, active_channel->channel);
    free(nickbuf);
}

void exit_cleanup()
{
    printf("Unlinking TTY ..\r\n");
}

void reset_nicklist(irc_session_t *s, char *channel)
{
    irc_ctx_t * ctx = irc_get_ctx(s);
    bufptr *message_buffer = channel_buffer(s, channel);

    clear_nicklist(message_buffer->nicklist);
    message_buffer->nicklist = NULL;
    message_buffer->nickcount = 0;

    return;
}

void clear_nicklist(nickname *nick)
{
    if(nick == NULL)
        return;
    else
        clear_nicklist(nick->next);

    free(nick->handle);
    free(nick);

    return;
}

void clear_msglist(bufline *message)
{
    if(message == NULL)
        return;
    else
        clear_msglist(message->next);

    free(message->message);
    free(message);
}

void clear_buffer(irc_session_t *s, bufptr *buffer)
{
    irc_ctx_t * ctx = irc_get_ctx(s);
    if (buffer != ctx->server_buffer)
    {
        buffer->prevbuf->nextbuf = buffer->nextbuf;
        if (buffer->nextbuf != NULL)
            buffer->nextbuf->prevbuf = buffer->prevbuf;
    }
    else if (buffer->nextbuf == NULL)
        ctx->server_buffer->prevbuf = buffer->prevbuf;
    
    clear_msglist(buffer->head);
    clear_nicklist(buffer->nicklist);
    free(buffer->channel);
    free(buffer->topic);
    free(buffer->topicsetby);
    free(buffer);
}

void clear_all(bufptr *buffer)
{
    if(buffer == NULL)
        return;
    else
        clear_all(buffer->nextbuf);

    clear_msglist(buffer->head);
    clear_nicklist(buffer->nicklist);
    free(buffer->channel);
    free(buffer->topic);
    free(buffer->topicsetby);
    free(buffer);

    return;
}
