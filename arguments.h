#pragma once

#include <argp.h>

/* Argument Parser */
extern const char *argp_program_version;
extern const char *argp_program_bug_address;
static char doc[] = "ircom IRC Client";
static char args_doc[] = "server channel";

static struct argp_option options[] =
{
    {"port", 'p', "port", 0, "Server port. Default: 6667"},
    {"nick", 'n', "nick", 0, "Nickname. Default: localhost username"},
    {"username", 'u', "username", 0, "Username. Default: localhost username"},
    {"realname", 'r', "\"real name\"", 0, "Real name. Default: localhost username"},
    {"enable-tls", 'e', 0, 0, "Enable TLS encryption (Requires libircclient with built-in openssl support)"},
    {"verify", 'v', 0, 0, "Verify server certificate identity when using TLS. Default: Do not verify server certificate"},
    {0}
};

struct arguments
{
    char *args[2];
    int port;
    char *nick;
    char *username;
    char *realname;
    int enable_tls;
    int verify;
};

error_t parse_opt(int, char *, struct argp_state*);

static struct argp argp = {options, parse_opt, args_doc, doc};
