# jtxsync
Utility used in conjunction with WSJT-X to adjust system clock using received FT8 messages delta time information.

THIS IS A WORK IN EARLY DEVELOPMENT AND NOT FULLY FUNCTIONAL.

Intended for Linux and MacOS systems. 
Status:
    Linux: Working
    MacOS Apple Silicon: Builds
    MacOS Intel: Builds
    
Runs in the Terminal.
Use of this utility will require root privleges via sudo to adjust the system clock.
After starting WSJT-X in FT8 mode and messages are being decoded, open a terminal and run the following command, "sudo jtxsync".
Once jtsync has gathered delta time from at least 10 messages, the user will be prompted to adjust the system clock.
Selecting Y for yes will update the system clock, N for no will not update the system clock, and Q for quit will exit the program.
Selecting N for no will cause jtxsync to gather 10 more messages and prompt the user to approve an adjustment.

Typical use would be to run jtxsync shortly after launching WSJT-X and updating the system clock to align better with incoming FT8 messages.

Installation:
to be continued....
