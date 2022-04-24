#include "functions.h"

bufptr *init_buffer(char *channel)
{
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
    channel_buffer->curr->prev = NULL;
    channel_buffer->curr->next = NULL;
    channel_buffer->curr->message = NULL;
    channel_buffer->curr->isread = NULL;
    channel_buffer->prevbuf = NULL;
    channel_buffer->nextbuf = NULL;
    if (server_buffer != NULL)
        server_buffer->prevbuf = channel_buffer;

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
bufptr *channel_buffer(char *channel)
{
    bufptr *search_ptr = server_buffer;

    if(strcmp(search_ptr->channel, channel) == 0)
        return search_ptr;

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

int channel_isjoined(char *channel)
{
    bufptr *search_ptr = server_buffer;

    while (search_ptr->nextbuf != NULL)
    {
        if(strcmp(search_ptr->nextbuf->channel, channel) == 0)
            return 1;

        search_ptr = search_ptr->nextbuf;
    }

    return 0;
}

int nick_is_member(char *channel, char *nick)
{
    nickname *search_ptr = channel_buffer(channel)->nicklist;
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

int add_member(char *channel, char *nick)
{
    if(!channel_isjoined(channel))
    {
        char errmsg[256];
        snprintf(errmsg, 256, "<error adding %s to nonexistant channel \'%s\'.>", nick, channel);
        add_to_buffer(server_buffer, errmsg);

        print_new_messages();
        return -1;
    }

    bufptr *chanbuf = channel_buffer(channel);
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
        search_ptr->mode = mode; // Update mode if it has changed
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

int delete_member(char *channel, char *nick)
{
    if(!channel_isjoined(channel))
    {
        char errmsg[256];
        snprintf(errmsg, 256, "<error removing %s from nonexistant channel \'%s\'.>", nick, channel);
        add_to_buffer(server_buffer, errmsg);

        print_new_messages();
        return -1;
    }

    bufptr *chanbuf = channel_buffer(channel);
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

int nickwidth_timer()
{
    time_t current_time = time(NULL);

    if (current_time > (nickwidth_set_at + 300))
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
    char nickbuf[128];
    bufptr *message_buffer = channel_buffer(ctx->active_channel);
    int nickwidth_tmp = message_buffer->nickwidth;
    if(message_buffer == server_buffer)
    {
        printf("<you can't send messages to the server buffer>\n");       
        return;
    }
    snprintf(nickbuf, 128, "\e[36;1m[%s]\e[0m", ctx->nick);
    if (nickwidth_timer() || strlen(nickbuf) > message_buffer->nickwidth)
    {
        message_buffer->nickwidth = strlen(nickbuf);
        nickwidth_set_at = time(NULL);
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
    bufptr *message_buffer = channel_buffer(ctx->active_channel);

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
    bufptr *active_channel = channel_buffer(ctx->active_channel);
    
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

void show_prompt(irc_ctx_t ctx)
{
    char nickbuf[128];
    snprintf(nickbuf, 128, "[%s]", ctx.nick);
    printf("%-*s ", (int)strlen(nickbuf)+2, nickbuf);
}

void kick_user(irc_session_t *s)
{
    irc_ctx_t *ctx = irc_get_ctx(s);
    bufptr *active_channel = channel_buffer(ctx->active_channel);

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
    input[strlen(input)-1] = '\0'; // Remove trailing newline

    return input;
}

void rewind_buffer(bufline *buffer_read_ptr, int lines)
{
    ioctl(0, TIOCGWINSZ, &ttysize);
                                   
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

void print_new_messages()
{
    if (input_wait == 0)
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
    }
}

void peek_channel(irc_session_t *s)
{
    irc_ctx_t *ctx = irc_get_ctx(s);
    bufptr *search_ptr = server_buffer;
    char *channel;
    bufptr *active_channel = channel_buffer(ctx->active_channel);

    printf("%-*s ", active_channel->nickwidth-11, ":peek>");
    channel = get_input();

    while (search_ptr->nextbuf != NULL)
    {
        if(strcmp(search_ptr->nextbuf->channel, channel) == 0)
        {
            rewind_buffer(search_ptr->nextbuf->curr, -1);
            return;
        }

        search_ptr = search_ptr->nextbuf;
    }

    printf("<no history available for \'%s\'>\r\n", channel);
    free(channel);

    return;
}

void exit_cleanup()
{
    clear_all(server_buffer);
    tcsetattr(0, TCSANOW, &termstate);
}

void reset_nicklist(char *channel)
{
    bufptr *message_buffer = channel_buffer(channel);

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

void clear_buffer(bufptr *buffer)
{
    if (buffer != server_buffer)
    {
        buffer->prevbuf->nextbuf = buffer->nextbuf;
        if (buffer->nextbuf != NULL)
            buffer->nextbuf->prevbuf = buffer->prevbuf;
    }
    else if (buffer->nextbuf == NULL)
        server_buffer->prevbuf = buffer->prevbuf;
    
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
