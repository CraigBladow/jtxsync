# jtxsync version 0.3
jtsync is a utility used in conjunction with WSJT-X to adjust the local system clock using received FT8 messages delta time information.

jtsync is intended for Linux and MacOS systems.  
Status:  
    Linux: Working\
    MacOS Apple Silicon: Working\
    MacOS Intel: Working
    
Runs in the Terminal.\
Use of this utility will require root privleges via sudo to adjust the system clock.\
After starting WSJT-X in FT8 mode and messages are being decoded, open a terminal in the directory holding jtsync and run the following command, "sudo ./jtxsync".\
Optionally adding '-n ###' where ### is a number from 4 to 100 the number of samples can be specified.  
Once jtsync has gathered delta time samples from 4 to 100 messages with 10 being the default, the user will be prompted to adjust the system clock.\
Selecting Y for yes will update the system clock, N for no will not update the system clock, and Q for quit will exit the program.\
Selecting N for no will cause jtxsync to gather more messages and prompt the user again to approve an adjustment.

Typical use would be to run jtxsync shortly after launching WSJT-X and updating the system clock to align better with incoming FT8 messages.

Installation:\
    Linux:\
        1. Install your distribution's 'build-essentials' or equivalent ensuring that 'git' and 'gcc' are installed.\
        2. git clone https://github.com/CraigBladow/jtxsync.git  
        3. cd jtxsync\
        4. ./bbuild.sh  builds the binary jtxsync in the jtsync/build directory.\
    MacOs:\
        1. Install MacOs command line tools see: (https://www.freecodecamp.org/news/install-xcode-command-line-tools/)  
        2. git clone https://github.com/CraigBladow/jtxsync.git  
        3. cd jtxsync  
        4. ./bbuild.sh  builds the binary jtxsync in the jtsync/build directory.         
            
