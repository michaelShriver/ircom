#include "handlers.h"

/*
bufptr *server_buffer;
bufline *buffer_read_ptr;
struct termios termstate;
struct termios termstate_raw;
struct winsize ttysize;
bool input_wait = 0;
bool output_wait = 0;
time_t nickwidth_set_at;
int port;
FILE *pager;
int errno;
*/

int main(int argc, char **argv)
{
    struct arguments arguments;
    arguments.port = 0;
    arguments.nick = NULL;
    arguments.username = NULL;
    arguments.realname = NULL;
    arguments.enable_tls = 0;
    arguments.noverify = 0;

    char keycmd;
    pthread_t event_thread;

    /* Initialize callbacks */
    irc_callbacks_t callbacks;

    /* Zero out memory allocation for callbacks struct */
    memset (&callbacks, 0, sizeof(callbacks));

    callbacks.event_connect = event_connect;
    callbacks.event_join = event_join;
    callbacks.event_nick = dump_event;
    callbacks.event_quit = event_quit;
    callbacks.event_part = event_part;
    callbacks.event_mode = dump_event;
    callbacks.event_topic = event_topic;
    callbacks.event_kick = event_kick;
    callbacks.event_channel = event_channel;
    callbacks.event_privmsg = event_privmsg;
    callbacks.event_notice = event_notice;
    callbacks.event_invite = dump_event;
    callbacks.event_umode = dump_event;
    callbacks.event_ctcp_rep = dump_event;
    callbacks.event_ctcp_action = event_action;
    callbacks.event_unknown = dump_event;
    callbacks.event_numeric = event_numeric;

    /* Create a new IRC session */
    irc_session_t *sess = irc_create_session(&callbacks);

    /* check for error */
    if (!sess)
    {
        fprintf (stderr, "Could not create session\n");
        exit(1);
    }

    /* Initialize user-defined IRC context, with two buffers */
    irc_ctx_t ctx;
    irc_set_ctx(sess, &ctx);

    /* On exit, clean up memory and reset terminal state */
    atexit(exit_cleanup);

    /* Set initial nickwidth timestamp */
    ctx.nickwidth_set_at = time(NULL);

    /* Save terminal state */
    ioctl(0, TIOCGWINSZ, &ctx.ttysize);
    tcgetattr(0, &ctx.termstate);
    memcpy(&ctx.termstate_raw, &ctx.termstate, sizeof(ctx.termstate_raw)); 
 
    /* Set initial nickwidth timestamp */
    ctx.nickwidth_set_at = time(NULL);

    /* Initialize buffers */
    ctx.server_buffer = init_buffer(sess, arguments.args[0]);

    /* Start output in the server buffer */
    ctx.buffer_read_ptr = ctx.server_buffer->curr;

    /* Process command line arguments */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    if (arguments.noverify)
    {
        irc_option_set(sess, LIBIRC_OPTION_SSL_NO_VERIFY);
    }

    /* Set Port */
    if (arguments.port <= 0 || arguments.port > 65535)
    {
        ctx.port = arguments.enable_tls==1 ? 6697 : 6667;
    }
    else
    {
        ctx.port = arguments.port;
    }

    /* Get OS username */
    char *OS_USERNAME = getlogin();

    if (arguments.nick == NULL)
    {
        memcpy(ctx.nick, OS_USERNAME, 128);
    }
    else
    {
        memcpy(ctx.nick, arguments.nick, 128);
    }

    if (arguments.username == NULL)
    {
        memcpy(ctx.username, OS_USERNAME, 128);
    }
    else
    {
        memcpy(ctx.username, arguments.username, 128);
    }

    if (arguments.realname == NULL)
    {
        memcpy(ctx.realname, OS_USERNAME, 128);
    }
    else
    {
        memcpy(ctx.realname, arguments.realname, 128);
    }

    if (arguments.args[1] == NULL )
    {
        ctx.active_channel = ctx.server_buffer->channel;
    }
    else
    {
        ctx.active_channel = arguments.args[1];
    }

    int serverlen = strlen(arguments.args[0]);
    char server[serverlen+2];

    if(arguments.enable_tls)
    {
        snprintf(server, serverlen+2, "#%s", arguments.args[0]);
    }
    else
    {
        memcpy(server, arguments.args[0], serverlen+1);
    }

    /* Initiate the IRC server connection */
    if (irc_connect(sess, server, ctx.port, 0, ctx.nick, ctx.username, ctx.realname))
    {
        fprintf (stderr, "Could not connect: %s\n", irc_strerror (irc_errno(sess)));
        exit(1);
    }

    /* Fork thread that runs libirc event handling loop */
    int err = pthread_create(&event_thread, NULL, irc_event_loop, (void *)sess);
    if (err)
    {
        fprintf (stderr, "FATAL: Failed thread IRC event handler\n");
        exit(1);
    }

    /* Set termstate to raw */
    cfmakeraw(&ctx.termstate_raw);

    /* Wait for the message buffer to be initialized with valid data */
    while (ctx.buffer_read_ptr->message == NULL)
    {
        sleep(1);
    }

    while (1)
    {
        tcsetattr(0, TCSANOW, &ctx.termstate_raw);
        char stroke = getchar();
        tcsetattr(0, TCSANOW, &ctx.termstate);

        switch(stroke)
        {
            case '<':
            {
                ctx.active_channel = channel_buffer(ctx.active_channel)->prevbuf->channel;
                ctx.buffer_read_ptr = channel_buffer(ctx.active_channel)->curr;
                if(channel_buffer(ctx.active_channel) == ctx.server_buffer)
                {
                    printf("[server messages]\n\n");
                }
                else
                {       
                    reset_nicklist(ctx.active_channel);
                    irc_cmd_names(sess, ctx.active_channel);
                }
                break;
            }
            case '>':
            {
                if (channel_buffer(ctx.active_channel)->nextbuf == NULL)
                {
                    ctx.active_channel = ctx.server_buffer->channel;
                    ctx.buffer_read_ptr = ctx.server_buffer->curr;
                    printf("[server messages]\n\n");
                }
                else
                {
                    ctx.active_channel = channel_buffer(ctx.active_channel)->nextbuf->channel;
                    ctx.buffer_read_ptr = channel_buffer(ctx.active_channel)->curr;
                    reset_nicklist(ctx.active_channel);
                    irc_cmd_names(sess, ctx.active_channel);
                }
                break;
            }
            case 'c':
            {
                printf("\e[1;1H\e[2J");
                break;
            }
            case 'C':
            {
                char *input;
                ctx.input_wait = 1;
                printf("%-*s ", channel_buffer(ctx.active_channel)->nickwidth-11, ":command>");
                input = get_input();
                irc_send_raw(sess, "%s", input);
                free(input);
                ctx.input_wait = 0;
                print_new_messages();
                break;
            }
            case 'd':
            {
                ctx.input_wait = 1;
                printf("Do you really want to take a dump of %s? (y/n) ", ctx.active_channel);
                tcsetattr(0, TCSANOW, &ctx.termstate_raw);
                char ans = getchar();
                tcsetattr(0, TCSANOW, &ctx.termstate);

                switch(ans)
                {
                    case 'y':
                    case 'Y':
                    {
                        printf("YES\n");
                        FILE *dumpfilep;
                        char dumpfilen[152];
                        char timebuf[9];
                        time_t now = time(&now);
                        struct tm *utc = gmtime(&now);
                        strftime(timebuf, 9, "%H.%M.%S", utc);
                        snprintf(dumpfilen, 128, "IRCOM_DUMP_%s_%s.txt", ctx.active_channel, timebuf);
                        bufline *dump_ptr = channel_buffer(ctx.active_channel)->head;

                        if((dumpfilep=fopen(dumpfilen, "ab")) != 0)
                        {
                            while(dump_ptr->next != NULL)
                            {
                                fprintf(dumpfilep, "%s\n", dump_ptr->message);
                                dump_ptr = dump_ptr->next;
                            }
                            fprintf(dumpfilep, "%s\n", dump_ptr->message);
                            fclose(dumpfilep);
                        }
                        printf("<dump taken>\n");

                        break;
                    }
                    default:
                    {
                        printf("NO\n<dump not taken>\n");
                    }
                }

                ctx.input_wait = 0;
                print_new_messages();
                break;
            }
            case 'e':
            {
                ctx.input_wait = 1;
                send_action(sess);
                ctx.input_wait = 0;
                print_new_messages();
                break;
            }
            case 'g':
            {
                char *input;
                ctx.input_wait = 1;
                printf("%-*s ", channel_buffer(ctx.active_channel)->nickwidth-11, ":goto>");
                input = get_input();
                if (channel_isjoined(input))
                {
                    bufptr *chanbuf = channel_buffer(input);
                    ctx.active_channel = chanbuf->channel;
                    ctx.buffer_read_ptr = chanbuf->curr;
                    irc_cmd_names(sess, ctx.active_channel);
                }
                else
                {
                    irc_cmd_join(sess, input, 0);
                }
                free(input);
                ctx.input_wait = 0;
                print_new_messages();
                break;
            }
            case 'h':
            case '?':
            {
                printf("\n");
                printf("irCOMMODE (c)2023 - Version .27");
                printf("\n\n  ");
                printf("%-36s", "\e[33;1mc\e[0m - clear");
                printf("%-36s", "\e[33;1mC\e[0m - issue raw command");
                printf("%-36s", "\e[33;1md\e[0m - dump out of ircom");
                printf("\n  ");
                printf("%-36s", "\e[33;1me\e[0m - emote");
                printf("%-36s", "\e[33;1mg\e[0m - goto a room");
                printf("%-36s", "\e[33;1mh\e[0m - command help");
                printf("\n  ");
                printf("%-36s", "\e[33;1mk\e[0m - kick a user");
                printf("%-36s", "\e[33;1ml\e[0m - list open rooms");
                printf("%-36s", "\e[33;1mp\e[0m - peek into room");
                printf("\n  ");
                printf("%-36s", "\e[33;1mP\e[0m - leave current room");
                printf("%-36s", "\e[33;1mq\e[0m - quit commode");
                printf("%-36s", "\e[33;1mr\e[0m - room history");
                printf("\n  ");
                printf("%-36s", "\e[33;1mR\e[0m - extended history");
                printf("%-36s", "\e[33;1ms\e[0m - send private");
                printf("%-36s", "\e[33;1mw\e[0m - who is in the room");
                printf("\n  ");
                printf("%-36s", "\e[33;1m<\e[0m - surf rooms backward");
                printf("%-36s", "\e[33;1m>\e[0m - surf rooms forward");
                printf("\n\n");
                printf("To begin TALK MODE, press [SPACE]");
                printf("\n\n");

                break;
            }
            case 'k':
            {
                kick_user(sess);
                break;
            }
            case 'l':
            {
                int fpstatus;
                int output_timeout = 0;

                ctx.output_wait = 1;
                ctx.pager = popen("more", "w");
                if (ctx.pager == NULL)
                {
                    fprintf(stderr, "Error opening pager\r\n");
                }
                irc_cmd_list(sess, NULL);
                while(ctx.output_wait)
                {
                    if (output_timeout > 100)
                        break;
                    output_timeout++;
                    sleep(.1);
                }
                tcsetattr(0, TCSANOW, &ctx.termstate);
                fpstatus = pclose(ctx.pager);
                if (fpstatus == -1)
                {
                    fprintf(stderr, "Pipe returned an error\r\n");
                }
                tcsetattr(0, TCSANOW, &ctx.termstate_raw);
                break;
            }
            case 'm':
            {
                break;    
            }
            case 'p':
            {
                ctx.input_wait = 1;
                peek_channel(sess);
                ctx.input_wait = 0;
                print_new_messages();
                break;
            }
            case 'P':
            {
                if(strcmp(ctx.server_buffer->channel, ctx.active_channel) == 0)
                    printf("<no channel is active>\r\n");
                else
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
                rewind_buffer(ctx.buffer_read_ptr, -1);
                break;
            }
            case 'R':
            {
                char *input;
                int lines;
                ctx.input_wait = 1;
                printf("%-*s ", channel_buffer(ctx.active_channel)->nickwidth-11, ":lines>");
                input = get_input();
                sscanf(input, "%d", &lines);
                free(input);
                rewind_buffer(ctx.buffer_read_ptr, lines);
                ctx.input_wait = 0;
                print_new_messages();
                break;
            }
            case 's':
            {
                ctx.input_wait = 1;
                send_privmsg(sess);
                ctx.input_wait = 0;
                print_new_messages();
                break;
            }
            case 'w':
            {
                reset_nicklist(ctx.active_channel);
                irc_cmd_names(sess, ctx.active_channel);

                break;
            }
            case '\r':
            case ' ':
            {
                ctx.input_wait = 1;
                send_message(sess);
                ctx.input_wait = 0;
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
