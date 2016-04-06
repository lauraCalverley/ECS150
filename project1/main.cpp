#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <ctype.h>
#include <fcntl.h>

#include <string>
#include <vector>
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

string executeBackspace(string command) {
    char *temp = "\b \b";
    write(STDOUT_FILENO, temp, strlen(temp));
    return (command.substr(0,(command.length()-1)));
}

vector<char *> parseParameters(string parameterString, char *token) {
    vector<char *> parameterVector;
    char const * parameterCharArray = parameterString.c_str();
    char *tokenFound = strtok((char *)parameterCharArray, token);
    
    while (parameterVector.empty() || parameterVector.back() != NULL) {
        parameterVector.push_back(strtok(NULL, token));
    }
    
    if (parameterVector.size() >= 1) {
        parameterVector.pop_back();
    }
    return parameterVector;
}


vector<vector<char *> > checkAdditionalParameters(string parameterString) {
    vector<vector<char *> > parameterVector;
    
    parameterVector.push_back(parseParameters(parameterString, "|")); // piping
    parameterVector.push_back(parseParameters(parameterString, "<")); // input
    parameterVector.push_back(parseParameters(parameterString, ">")); // output
    
    return parameterVector;
}


void executeInvalidCommand(string command) {
    printNewLine();
    char *temp = "Failed to execute ";
    write(STDOUT_FILENO, temp, strlen(temp));
    write(STDOUT_FILENO, command.c_str(), strlen(command.c_str()));
    printNewLine();
}



void executePwd(string parameterString) { // to be forked?
    char *directoryName = NULL;
    directoryName = getcwd(directoryName, 0);
    printNewLine();

    vector<vector<char *> > parameters = checkAdditionalParameters(parameterString);
    
    if (!(parameters[0].empty()) && parameters[0][0] != NULL) { //pipe
        cout << "pipe" << endl;
    }
    
    if (!(parameters[2].empty()) && parameters[2][0] != NULL) { //redirect output
        // open a files for output
        cout << "in output redirect if" << endl;
        for (int i=0; i < parameters[2].size(); i++) {
            cout << "parameter" << parameters[2][i] << "endparam" << endl;
        }
        int flags = O_CREAT | S_IRUSR | S_IWUSR; //FIXME // add | O_RDWR
        //int flags = O_CREAT; //FIXME
        int outputFD;
        
        for (int i=0; i < parameters[2].size(); i++) {
            // create a file for each output vector entry
            outputFD = open(parameters[2][i], flags); // returns the new file descriptor
            close(outputFD);
        }
            
        // write to final output vector entry
        flags = S_IRUSR | S_IWUSR | O_RDWR;
        outputFD = open(parameters[2][(parameters[2].size() - 1)], flags); // returns the new file descriptor
        write(outputFD, directoryName, strlen(directoryName));
    }
    
    else {
        cout << "in else" << endl;
        write(STDOUT_FILENO, directoryName, strlen(directoryName));
        printNewLine();
    }
    
    
}

void executeCd(string parameterString) {
    if ((parameterString.length() == 0) || (parameterString[0] == ' ')) {
        printNewLine();
        
        char const *directoryName;
        
        if (parameterString.length() == 0) {
            directoryName = getenv("HOME");
        }
        else {
            directoryName = (parameterString.substr(1, parameterString.length()-1)).c_str();
        }
        
        int success = chdir(directoryName);
        
        if (success == -1) {
            char *errorMessage = "Error changing directory.";
            write(STDOUT_FILENO, errorMessage, strlen(errorMessage));
            printNewLine();
        }
    }
    
    else {
        executeInvalidCommand("");
    }

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

void directCommand(string command) {
    string commandType = command.substr(0,3); // FIXME
    string parameterString;
    
    if (command.length() > 2) {
        parameterString = command.substr (2, (command.length() - 2));
    }
    
    if (commandType == "cd") {
        executeCd(parameterString);
    }
    else if (commandType == "ls") {
        cout << "ls" << endl;
    }
    else if (commandType == "pwd") {
        executePwd(parameterString);
    }
    else if (commandType == "ff") {
        cout << "ff" << endl;
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
                while(nextChar != 0x0A){//while not enter
                    switch(nextChar) {
                        case 0x08: //backspace
                        case 0x7F: { //delete
                            command = executeBackspace(command); // returns command to remove last char from command string
                            break;
                        }
                        case 0x1B: { // escape character
                            cout << "arrow" << endl;
                            break;
                        }
                        default: { // input chars
                            write(STDOUT_FILENO, &nextChar, 1);
                            command += nextChar;
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

