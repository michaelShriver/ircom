#include "handlers.h"

bufptr *server_buffer;
bufline *buffer_read_ptr;
struct termios termstate;
struct winsize ttysize;
bool input_wait = 0;

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

    /* Save terminal state */
    ioctl(0, TIOCGWINSZ, &ttysize);
    struct termios termstate_raw;
    tcgetattr(0, &termstate);
    memcpy(&termstate_raw, &termstate, sizeof(termstate_raw)); 

    /* On exit, clean up memory and reset terminal state */
    atexit(exit_cleanup);

    /* Zero out memory allocation for callbacks struct */
    memset (&callbacks, 0, sizeof(callbacks));

    callbacks.event_connect = event_connect;
    callbacks.event_join = event_join;
    callbacks.event_nick = dump_event;
    callbacks.event_quit = event_quit;
    callbacks.event_part = event_part;
    callbacks.event_mode = dump_event;
    callbacks.event_topic = event_topic;
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
    irc_option_set(sess, LIBIRC_OPTION_SSL_NO_VERIFY);

    /* check for error */
    if ( !sess )
    {
        printf ("Could not create session\n");
        exit(1);
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
        exit(1);
    }

    /* Fork thread that runs libirc event handling loop */
    int err = pthread_create(&event_thread, NULL, irc_event_loop, (void *)sess);
    if (err)
    {
        printf ("Failed to create thread\n");
        exit(1);
    }

    /* Set termstate to raw */
    cfmakeraw(&termstate_raw);

    /* Wait for the message buffer to be initialized with valid data */
    while (buffer_read_ptr->message == NULL)
    {
        sleep(1);
    }

    while (1)
    {
        tcsetattr(0, TCSANOW, &termstate_raw);
        char stroke = getchar();
        tcsetattr(0, TCSANOW, &termstate);

        switch(stroke)
        {
            case '<':
            {
                strcpy(ctx.active_channel, channel_buffer(ctx.active_channel)->prevbuf->channel);
                buffer_read_ptr = channel_buffer(ctx.active_channel)->curr;
                if(channel_buffer(ctx.active_channel) == server_buffer)
                {
                    printf("[server messages]\n\n");
                }
                else
                {       
                    printf("[you are in \'%s\']\n\n", ctx.active_channel);
                    irc_cmd_names(sess, ctx.active_channel);
                }
                break;
            }
            case '>':
            {
                if (channel_buffer(ctx.active_channel)->nextbuf == NULL)
                {
                    strcpy(ctx.active_channel, server_buffer->channel);
                    buffer_read_ptr = server_buffer->curr;
                    printf("[server messages]\n\n");
                }
                else
                {
                    strcpy(ctx.active_channel, channel_buffer(ctx.active_channel)->nextbuf->channel);
                    buffer_read_ptr = channel_buffer(ctx.active_channel)->curr;
                    printf("[you are in \'%s\']\n\n", ctx.active_channel);
                    irc_cmd_names(sess, ctx.active_channel);
                }
                break;
            }
            case 'c':
            {
                printf("\e[1;1H\e[2J");
                break;
            }
            case 'e':
            {
                char *input;
                input_wait = 1;
                printf("%-*s", channel_buffer(ctx.active_channel)->nickwidth, ":emote> ");
                input = get_input();
                send_action(sess, input);
                free(input);
                input_wait = 0;
                print_new_messages();
                break;
            }
            case 'g':
            {
                char *input;
                input_wait = 1;
                printf("%-*s", channel_buffer(ctx.active_channel)->nickwidth, ":channel> ");
                input = get_input();
                if (channel_isjoined(input))
                {
                    strcpy(ctx.active_channel, input);
                    buffer_read_ptr = channel_buffer(ctx.active_channel)->curr;
                    printf("[you are in \'%s\']\n\n", ctx.active_channel);
                    irc_cmd_names(sess, ctx.active_channel);
                }
                else
                {
                    irc_cmd_join(sess, input, 0);
                }
                free(input);
                input_wait = 0;
                print_new_messages();
                break;
            }
            case 'p':
            {
                char *input;
                input_wait = 1;
                printf("%-*s", channel_buffer(ctx.active_channel)->nickwidth, ":peek> ");
                input = get_input();
                peek_channel(input);
                free(input);
                input_wait = 0;
                print_new_messages();
                break;
            }
            case 'P':
            {
                irc_cmd_part(sess, ctx.active_channel);
                break;
            }
            case 'q':
            case 3:
            {
                exit(0);
            }
            case 'r':
            {
                rewind_buffer(buffer_read_ptr, -1);
                break;
            }
            case 'R':
            {
                char *input;
                int lines;
                input_wait = 1;
                printf("%-*s", channel_buffer(ctx.active_channel)->nickwidth, ":lines> ");
                input = get_input();
                sscanf(input, "%d", &lines);
                free(input);
                rewind_buffer(buffer_read_ptr, lines);
                input_wait = 0;
                print_new_messages();
                break;
            }
            case 'w':
            {
                printf("[you are in \'%s\']\n\n", ctx.active_channel);
                irc_cmd_names(sess, ctx.active_channel);

                break;
            }
            case '\r':
            case ' ':
            {
                char *input;
                input_wait = 1;
                send_message(sess, ctx.active_channel);
                input_wait = 0;
                print_new_messages();
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return 1;
}
