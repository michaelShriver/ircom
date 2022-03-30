# ircom

A Blessed IRC Client

ircom is a comand-line IRC client implemented without curses. The client is designed with slow-to-update serial terminals and physical teletypes in mind. All of it's output is printed line-by-line, with no screen refreshes. Curses-based clients update the entire screen with every line, which is problematic on low-capability vintage hardware.

The control scheme is modeled after SDF's [commode](http://jwodder.freeshell.org/sdf/commands.html) script. (to be implemented)
