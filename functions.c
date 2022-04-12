#include "functions.h"

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

    buffer->prevbuf->nextbuf = buffer->nextbuf;
    if (buffer->nextbuf != NULL)
        buffer->nextbuf->prevbuf = buffer->prevbuf;
    free(buffer->head->message);
    free(buffer->head);
    free(buffer->channel);
    free(buffer);
}

void clear_all()
{
    bufptr *search_ptr = server_buffer;

    while (search_ptr->nextbuf != NULL)
        search_ptr = search_ptr->nextbuf;

    while (search_ptr->prevbuf != NULL)
    {
        search_ptr = search_ptr->prevbuf;
        clear_buffer(search_ptr->nextbuf);
    }

    /* Clean up the server_buffer */
    while(server_buffer->curr->prev != NULL)
    {
        server_buffer->curr = server_buffer->curr->prev;
        free(server_buffer->curr->next->message);
        free(server_buffer->curr->next);
        server_buffer->curr->next = NULL;
    }

    free(server_buffer->head->message);
    free(server_buffer->head);
    free(server_buffer->channel);
    free(server_buffer);

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
