#include "handlers.h"

bufptr *server_buffer;
bufline *buffer_read_ptr;
struct termios termstate;
struct winsize ttysize;

int main(int argc, char **argv)
{
    if ( argc != 4 )
    {
        printf ("Usage: %s <server> <nick> <channel>\n", argv[0]);
        return 1;
    }

    /* Initialize callbacks and pointers */
    irc_callbacks_t callbacks;
    irc_session_t *sess;
    unsigned short port = 6667; // TODO: accept port from command line
    char keycmd;
    pthread_t event_thread;

    /* Save terminal state, and restore on exit */
    ioctl(0, TIOCGWINSZ, &ttysize);
    struct termios termstate_raw;
    tcgetattr(0, &termstate);
    memcpy(&termstate_raw, &termstate, sizeof(termstate_raw)); 
    atexit(exit_cleanup);

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
                exit(0);
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
                if (channel_isjoined(input))
                {
                    strcpy(ctx.active_channel, input);
                    buffer_read_ptr = channel_buffer(ctx.active_channel)->curr;
                    printf("<now chatting in \'%s\'>\r\n", ctx.active_channel);
                }
                else
                {
                    irc_cmd_join(sess, input, 0);
                }
                free(input);
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
}
