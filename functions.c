#include "functions.h"

bufptr *init_buffer(char *channel)
{
    char *bufchan = malloc(sizeof(char) * (strlen(channel) + 1));
    strcpy(bufchan, channel);

    bufptr *channel_buffer = malloc(sizeof(struct bufptr));
    channel_buffer->head = channel_buffer->curr = (struct bufline*) malloc(sizeof(struct bufline));
    channel_buffer->channel = bufchan;
    channel_buffer->topic = NULL;
    channel_buffer->topicsetby = NULL;
    channel_buffer->nickwidth = 0;
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

/* Add a message to the end of the message buffer, return a pointer to new entry */
bufline *add_to_buffer(bufptr *msgbuffer, char *cmsg)
{
    char *messageline = (char*)malloc(sizeof(char) * (strlen(cmsg) + 1));
    strcpy(messageline, cmsg);

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
    bufline *newmsg = (struct bufline*) malloc(sizeof(struct bufline));
    msgbuffer->curr->next = newmsg;
    newmsg->prev = msgbuffer->curr;
    newmsg->next = NULL;
    newmsg->message = messageline;
    newmsg->isread = 0;

    return newmsg;
}

/* Send a message to the channel, and add it to my buffer */
// void send_message(irc_session_t *s, char *channel, char *message)
void send_message(irc_session_t *s, char *channel)
{
    irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (s);
    char *input;
    char nickbuf[128];
    bufptr *message_buffer = channel_buffer(channel); //CHANGE THIS AND BELOW
    snprintf(nickbuf, 128, "\e[36;1m[%s]\e[0m", ctx->nick);
    message_buffer->nickwidth = strlen(nickbuf) > message_buffer->nickwidth ? strlen(nickbuf) : message_buffer->nickwidth;

    printf("%-*s ", message_buffer->nickwidth, nickbuf);
    input = get_input();
    if (strcmp(input, "") == 0)
    {
        free(input);
        printf("<no message sent>\n");
        return;
    }


    int size = sizeof(char) * (strlen(ctx->nick) + strlen(input) + message_buffer->nickwidth);
    char *bufferline = (char *)malloc(size);

    snprintf(bufferline, size, "%-*s %s", message_buffer->nickwidth, nickbuf, input);
    irc_cmd_msg(s, channel, input);
    message_buffer->curr = add_to_buffer(message_buffer, bufferline);
    message_buffer->curr->isread = 1;
    free(input);
    free(bufferline);
}

/* Send an action to the channel, and add it to my buffer */
void send_action(irc_session_t *s, char *action)
{
    irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (s);

    int size = sizeof(char) * (strlen(ctx->nick) + strlen(action) + 15);
    char *bufferline = (char *)malloc(size);
    bufptr *message_buffer = channel_buffer(ctx->active_channel);

    snprintf(bufferline, size, "\e[32;1m<%s %s>\e[0m", ctx->nick, action);
    irc_cmd_me(s, ctx->active_channel, action);
    message_buffer->curr = add_to_buffer(message_buffer, bufferline);
    message_buffer->curr->isread = 1;
    free(bufferline);
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

    server_buffer->curr = add_to_buffer(server_buffer, buf);

    if ( (fp = fopen ("irctest.log", "ab")) != 0 )
    {
        fprintf (fp, "%s\n", buf);
        fclose (fp);
    }
    print_new_messages();
}

/* Implement non-blocking character input kbhit() */
/*
int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
}
*/

void exit_cleanup()
{
    clear_all();
    tcsetattr(0, TCSANOW, &termstate);
}

/* custom functions for testing */
void show_prompt(irc_ctx_t ctx)
{
    char nickbuf[128];
    snprintf(nickbuf, 128, "[%s]", ctx.nick);
    printf("%-*s ", (int)strlen(nickbuf)+2, nickbuf);
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

void peek_channel(char *channel)
{
    bufptr *search_ptr = server_buffer;

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
        free(buffer_entry->next->message);
        free(buffer_entry->next);
        buffer_entry->next = NULL;
    }

    if (buffer != server_buffer)
       buffer->prevbuf->nextbuf = buffer->nextbuf;
    if (buffer->nextbuf != NULL)
        buffer->nextbuf->prevbuf = buffer->prevbuf;
    else if (buffer->nextbuf == NULL)
        server_buffer->prevbuf = buffer->prevbuf;
    free(buffer->head->message);
    free(buffer->head);
    free(buffer->channel);
    free(buffer->topic);
    free(buffer->topicsetby);
    free(buffer);
}

void clear_all()
{
    bufptr *search_ptr = server_buffer;

    while (search_ptr->nextbuf != NULL)
    {
        search_ptr = search_ptr->nextbuf;
        clear_buffer(search_ptr->prevbuf);
    }

    while(search_ptr->curr->prev != NULL)
    {
        search_ptr->curr = search_ptr->curr->prev;
        free(search_ptr->curr->next->message);
        free(search_ptr->curr->next);
        search_ptr->curr->next = NULL;
    }

    free(search_ptr->head->message);
    free(search_ptr->head);
    free(search_ptr->channel);
    free(search_ptr->topic);
    free(search_ptr->topicsetby);
    free(search_ptr);

    return;
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
