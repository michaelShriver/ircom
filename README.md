# ircom

A Blessed IRC Client

ircom is a comand-line IRC client implemented without curses. The client is designed with slow-to-update serial terminals and physical teletypes in mind. All of it's output is printed line-by-line, with no screen refreshes. Curses-based clients update the entire screen with every line, which is problematic on low-capability vintage hardware.

The control scheme is modeled after SDF's [commode](http://jwodder.freeshell.org/sdf/commands.html) script.

# Implemented commands:

        irCOMMODE (c)2023 - Version .27

          c - clear                d - dump out of com      e - emote
          g - goto a room          h - command help         k - kick a user 
          l - list open rooms      p - peek into room       q - quit commode 
          r - room history         R - extended history     s - send private
          w - who is in the room   < - surf rooms backward  > - surf rooms forward

        To begin TALK MODE, press [SPACE]

# Installation

Building requires libircclient:

    apt install libircclient-dev

Clone the source repo and make:

    git clone https://github.com/michaelshriver/ircom
    make && make install

# Installation on the MetaArray

Download and install libircclient locally from source:

[https://sourceforge.net/projects/libircclient/](https://sourceforge.net/projects/libircclient/files/libircclient/1.10/libircclient-1.10.tar.gz/download)

    tar xvzf libircclient-1.10.tar.gz && cd libircclient-1.10
    ./configure --enable-shared --prefix=$HOME/.local --libdir=$HOME/.local/lib
    make && make install

Update env variables: (consider adding these to your .profile)

    export C_INCLUDE_PATH="$C_INCLUDE_PATH:$HOME/.local/include"
    export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$HOME/.local/lib"
    export PATH="$PATH:$HOME/.local/bin"

And then build ircom:

    git clone https://github.com/michaelShriver/ircom.git
    cd ircom
    make CFLAGS="-I$HOME/.local/include -L$HOME/.local/lib"
    make PREFIX=$HOME/.local install

# Execution

    Usage: ircom <server> <nick> <channel>
