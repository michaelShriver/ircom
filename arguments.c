#include <stdlib.h>
#include "arguments.h"

const char *argp_program_version = "ircom v .27";
const char *argp_program_bug_address = "<shriver@sdf.org>";

error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;

    switch(key)
    {
        case 'p':
        {
            arguments->port = atoi(arg);
            break;
        }
        case 'n':
        {
            arguments->nick = arg;
            break;
        }
        case 'u':
        {
            arguments->username = arg;
            break;
        }
        case 'r':
        {
            arguments->realname = arg;
            break;
        }
        case 't':
        {
            arguments->enable_tls = 1;
            break;
        }
        case 'v':
        {
            arguments->noverify = 1;
            break;
        }
        case ARGP_KEY_ARG:
        {
            // Too many arguments, if your program expects only one argument.
            if(state->arg_num > 1)
                argp_usage(state);
            arguments->args[state->arg_num] = arg;
            break;
        }
        case ARGP_KEY_END:
        {
            // Not enough arguments. if your program expects exactly one argument.
            if(state->arg_num < 1)
                argp_usage(state);
            break;
        }
        default:
        {
            return ARGP_ERR_UNKNOWN;
        }
    }

    return 0;

}