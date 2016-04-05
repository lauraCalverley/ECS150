#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <ctype.h>

#include <string>
#include <cstring>
#include <iostream>

using namespace std;

void SetNonCanonicalMode(int fd, struct termios *savedattributes){
    struct termios TermAttributes;
    
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

void printNewLine() {
    char newLine = 0x0A;
    write(STDOUT_FILENO, &newLine, 1);
}

void executePwd() { // to be forked?
    char *directoryName = NULL;
    directoryName = getcwd(directoryName, 0);
    printNewLine();
    write(STDOUT_FILENO, directoryName, strlen(directoryName));
    printNewLine();
}

void printShellPrompt() {
    char *path = NULL;
    path = getcwd(path, 0);
    
    char *temp;
    char *currentDirectory = new char [strlen(path)+1];
    
    if (strlen(path) > 16) {
        temp = "/.../";
        write(STDOUT_FILENO, temp, strlen(temp));
        
        temp = strtok(path, "/");
        strcpy(currentDirectory, temp);
        while (temp != NULL)
        {
            strcpy(currentDirectory, temp);
            temp = strtok (NULL, "/");
        }
        
        write(STDOUT_FILENO, currentDirectory, strlen(currentDirectory));
        
    }
    else {
        write(STDOUT_FILENO, path, strlen(path));
    }
    
    temp = "% ";
    write(STDOUT_FILENO, temp, strlen(temp));
}

void executeInvalidCommand(string command) {
    printNewLine();
    char *temp = "Failed to execute ";
    write(STDOUT_FILENO, temp, strlen(temp));
    write(STDOUT_FILENO, command.c_str(), strlen(command.c_str()));
    printNewLine();
}

void directCommand(string command) {
    if (command == "cd") {
        
    }
    else if (command == "ls") {
        
    }
    else if (command == "pwd") {
        executePwd();
    }
    else if (command == "ff") {
        
    }
    else if (command == "exit") {
        printNewLine();
    }
    else {
        executeInvalidCommand(command);
    }
}

int main() {
	struct termios SavedTermAttributes;
	SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);

	char nextChar;
    string command;
	while(command != "exit"){ //breaks on exit if statement
                // print shell prompt
                printShellPrompt();
                read(STDIN_FILENO, &nextChar, 1);
                command = "";
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
                            //cout << "other char" << endl;
                            write(STDOUT_FILENO, &nextChar, 1);
                            command += nextChar;
                            //cout << command << endl;
                            
                            break;
                        }

                    }
                    read(STDIN_FILENO, &nextChar, 1);

                }
        directCommand(command);
	}

	ResetCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
        return 1;
}




//References:
//  Nitta noncanmode.c
// http://www.ascii-code.com/

