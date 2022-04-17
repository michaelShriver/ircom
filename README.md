# ircom

A Blessed IRC Client

ircom is a comand-line IRC client implemented without curses. The client is designed with slow-to-update serial terminals and physical teletypes in mind. All of it's output is printed line-by-line, with no screen refreshes. Curses-based clients update the entire screen with every line, which is problematic on low-capability vintage hardware.

The control scheme is modeled after SDF's [commode](http://jwodder.freeshell.org/sdf/commands.html) script.

# Features to implement:
marked with a (*)

        irCOMMODE (c)2023 - Version .27
        
          c - clear               *d - dump out of com      e - emote
         *E - toggle echo          g - goto a room          h - command help         
         *i - ignore a user       *k - kick a user         *l - list open rooms
         *m - mute user toggle     p - peek into room       q - quit commode
          r - room history         R - extended history    *s - send private         
          w - who is in the room   < - surf rooms backward  > - surf rooms forward
        
        To begin TALK MODE, press [SPACE]

