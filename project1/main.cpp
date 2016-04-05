#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <ctype.h>

#include <string>
#include <iostream>

using namespace std;


void SetNonCanonicalMode(int fd, struct termios *savedattributes){
    struct termios TermAttributes;
    char *name;
    
    // Make sure stdin is a terminal. 
    if(!isatty(fd)){
        fprintf (stderr, "Not a terminal.\n");
        exit(0);
    }
    
    // Save the terminal attributes so we can restore them later. 
    tcgetattr(fd, savedattributes);
    
    // Set the funny terminal modes. 
    tcgetattr (fd, &TermAttributes);
    TermAttributes.c_lflag &= ~(ICANON | ECHO); // Clear ICANON and ECHO. 
    TermAttributes.c_cc[VMIN] = 1;
    TermAttributes.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSAFLUSH, &TermAttributes);
}

void ResetCanonicalMode(int fd, struct termios *savedattributes){
    tcsetattr(fd, TCSANOW, savedattributes);
}

int main() {
	struct termios SavedTermAttributes;
	SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);

	char nextChar;
	//commands
	//while(1){ //not exit
		//characters
		
                read(STDIN_FILENO, &nextChar, 1);
                
                while(nextChar != 0x0A){//not enter
                    switch(nextChar) {
                        case 0x08: //backspace
                        case 0x7F: { //delete
                            cout << "delete/backspace" << endl;
                            break;
                        }
                        case 0x1B: { // escape character
                            cout << "arrow" << endl;
                            break;
                        }
                        default: { // input chars
                            cout << "other char" << endl;
                            break;
                        }

                    }


                    read(STDIN_FILENO, &nextChar, 1);

                }
	//}

	ResetCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
        return 1;
}

//References:
//  Nitta noncanmode.c
