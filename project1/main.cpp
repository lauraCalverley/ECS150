#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <ctype.h>
#include <fcntl.h>

#include <string>
#include <vector>
#include <deque>
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

string executeBackspace(string command) { //FIXME: don't allow prompt to be erased...audible bell
    if (command != "") {
        char *temp = "\b \b";
        write(STDOUT_FILENO, temp, strlen(temp));
        command = command.substr(0,(command.length()-1));
    }
    else {
        char audible = 0x07;
        write(STDOUT_FILENO, &audible, 1);
    }
    return command;
}

vector<char *> parseParameters(string parameterString, char *token) {
    vector<char *> parameterVector;
    char const *parameterCharArray = parameterString.c_str();
    strtok((char *)parameterCharArray, token);
    
    while (parameterVector.empty() || parameterVector.back() != NULL) {
        parameterVector.push_back(strtok(NULL, token));
    }
    
    //cout << "back: " << parameterVector[(parameterVector.size() - 1)] << endl;
    
    if (parameterVector.size() >= 1) { //&& (parameterVector.back() == NULL)) {
        parameterVector.pop_back();
    }
    
    for (int i=0; i < parameterVector.size(); i++) {
        cout << "parameterVector " << i << "for " << token << parameterVector[i] << endl;
    }
    
    return parameterVector;
}


vector<vector<char *> > checkAdditionalParameters(string parameterString) {
    vector<vector<char *> > parameterVector;
    
    parameterVector.push_back(parseParameters(parameterString, "| ")); // piping
    parameterVector.push_back(parseParameters(parameterString, "< ")); // input
    parameterVector.push_back(parseParameters(parameterString, "> ")); // output
    
    //parameterVector.push_back(parseParameters(parameterString, "|<> "));
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
        for (int i=0; i < parameters[0].size(); i++) {
            cout << "parameters[pipe]: " << parameters[0][i] << "end pipe param" << endl;
        }
    }
    if (parameters[0].size() == 0) {
        cout << "empty" << endl;
    }
    
    for (int i=0; i < parameters[0].size(); i++) {
        cout << "parameters[|]: " << parameters[0][i] << "end | param" << endl;
    }

    
    
    if (parameters[1].size() == 0) {
        cout << "empty" << endl;
    }
    
    for (int i=0; i < parameters[1].size(); i++) {
        cout << "parameters[<]: " << parameters[1][i] << "end < param" << endl;
    }
    
    
    if (parameters[2].size() == 0) {
        cout << "empty" << endl;
    }

    for (int i=0; i < parameters[2].size(); i++) {
        cout << "parameters[>]: " << parameters[2][i] << "end > param" << endl;
    }
    
    if (!(parameters[2].empty()) && parameters[2][0] != NULL) { //redirect output
        cout << "in the if" << endl;
        // open a files for output
        for (int i=0; i < parameters[2].size(); i++) {
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
    
    char const *commandC = command.c_str();
    string commandType = strtok((char *)commandC, " ");
    
    //cout << "commandType: " << commandType << endl;
    
    //string commandType = command.substr(0,3); // FIXME
    string parameterString;
    
    int commandLength = commandType.length();
    if (command.length() > commandLength) {
        parameterString = command.substr (commandLength, (command.length() - commandLength));
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

string executeArrows(deque<string> history, int &counter) {
    char nextChar;
    char audible = 0x07;
    string command;
    
    read(STDIN_FILENO, &nextChar, 1);
    if (nextChar == 0x5B) { // [
    
        read(STDIN_FILENO, &nextChar, 1);
        if (nextChar == 0x41) { // up arrow
            counter++;
            // cout << "counter: " << counter << endl;
            // cout << "history.size: " << history.size() << endl;
            // cout << "history.size - 1: " << history.size() - 1 << endl;
            // cout << "bool counter > history.size"
            if (counter > 9) {
                write(STDOUT_FILENO, &audible, 1);
                counter = 9;
                command = history[9];
            }
            else if (counter > (history.size()-1)) {
                cout << "right place\n";
                write(STDOUT_FILENO, &audible, 1);
                if(!history.size()){
                    counter = 0;
                    command = "";
                }
                else {
                    counter--;
                    command = history[counter];
                }
            }
            else {
                cout << "here\n";
                command = history[counter];
            }
            
        }
        else if (nextChar == 0x42) { // down arrow
            counter--;
            if (counter == -1) {
                write(STDOUT_FILENO, &audible, 1);
                counter = 0;
                if(!history.size()){
                    command = "";
                }
                else{
                    command = history[0];
                }
            }
            else {
                command = history[counter];
            }
            
        }
    }
    
    return command;
}

int main() {
	struct termios SavedTermAttributes;
	SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);

	char nextChar;
    string command;

    int counter = 0; // tracks how far back in history user is
    deque<string> history; //used to keep history of up to 10 previous commands // front is recent, back is old
    
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
                        case 0x1B: { // escape character // FIXME need prompt for history items
                            command = executeArrows(history, counter);
                            char *commandCString = (char *)command.c_str();
                            write(STDOUT_FILENO, &commandCString, strlen(commandCString));
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
        history.push_front(command);
        if (history.size() > 10) {
            history.pop_back();
        }
        counter = 0;
	}
    
    for (int i=0; i < history.size(); i++) {
        cout << history[i] << endl;
    }
    
	ResetCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
    
        return 1;
}


/* TO DO
 up and down arrows are causing seg faults
 need prompt for history items
 fix |<> tokenizing parameters
 execute other apps like grep, cat, etc. (execvp???) / get piping working
 get input redirection working - try hard coding a fake file name!
 get output redirection working
 add forking to pwd
 ls
 ff
 complete the README
 write the makefile
*/


//References:
//  Nitta noncanmode.c
// http://www.ascii-code.com/

