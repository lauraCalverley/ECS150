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

string executeBackspace(string command) {
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

vector<char *> parseParameters(string parameterString, char *token) { // assumes a non-empty parameter string
    vector<char *> parameterVector;

    char const *parameterCharArray = parameterString.c_str();
    char *mytok = strtok((char *)parameterCharArray, token);
    cout << "mytok (before while): " << mytok << endl;
    
    if (mytok) {
        parameterVector.push_back(mytok);
    }
    
    while (mytok = strtok(NULL, token)
           parameterVector.empty() || parameterVector.back() != NULL) {
        cout << "in while" << endl;
        char *mytok = strtok(NULL, token);
        cout << "mytok: " << mytok << endl;
        parameterVector.push_back(mytok);
    
    }

/* 
 while (parameterVector.empty() || parameterVector.back() != NULL) {
 cout << "in while" << endl;
 char *mytok = strtok(NULL, token);
 cout << "mytok: " << mytok << endl;
 parameterVector.push_back(mytok);
 
 }
*/
 
 
    //cout << "back: " << parameterVector[(parameterVector.size() - 1)] << endl;
    
    if (parameterVector.size() >= 1) { //&& (parameterVector.back() == NULL)) {
        parameterVector.pop_back();
    }
    
    for (int i=0; i < parameterVector.size(); i++) {
        cout << "parameterVector " << i << "for " << token << parameterVector[i] << endl;
    }
    
    return parameterVector;
}


vector<vector<char *> > checkAdditionalParameters(string parameterString) { // change it to parse all parameters here, no separate calls
    
    // delimit on pipes and spaces and push to parameterVector[0]
    
    
    vector<vector<char *> > parameterVector;
    vector<char *> dummy;
    
    if (!parameterString.empty()) {
        parameterVector.push_back(parseParameters(parameterString, "| ")); // piping
        parameterVector.push_back(parseParameters(parameterString, "< ")); // input
        parameterVector.push_back(parseParameters(parameterString, "> ")); // output
    }
    else {
        parameterVector.push_back(dummy); // piping
        parameterVector.push_back(dummy); // input
        parameterVector.push_back(dummy); // output
    }
    
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
    else {
        parameterString = "";
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

void executeArrows(deque<string> history, string &command, int &counter) {
    char nextChar;
    char audible = 0x07;
    
    read(STDIN_FILENO, &nextChar, 1);
    if (nextChar == 0x5B) { // [
    
        read(STDIN_FILENO, &nextChar, 1);
        if (nextChar == 0x41) { // up arrow
            counter++;
            if (counter > 9) { // = 10 case
                write(STDOUT_FILENO, &audible, 1);
                counter = 9;
                command = history[9];
            }
            else if (counter >= history.size()) {
                write(STDOUT_FILENO, &audible, 1);
                counter--;
            }
            else if (!history.size()){ //no history
                write(STDOUT_FILENO, &audible, 1);
                counter = -1;
            }
            else {
                // erase current user input
                int previousCommandLength = command.length();
                for (int i=0; i < previousCommandLength; i++) {
                    char *temp = "\b \b";
                    write(STDOUT_FILENO, temp, strlen(temp));
                }
                
                // write requested historical command
                command = history[counter];
                char *commandCString = (char *)command.c_str();
                write(STDOUT_FILENO, commandCString, strlen(commandCString));
            }
        }
        else if (nextChar == 0x42) { // down arrow
            counter--;
            if (counter == -2) { //FIXME: case where the user types, then hits UP, then hits DOWN (enter is never pressed)
                write(STDOUT_FILENO, &audible, 1);
                counter = -1;
            }
            else {
                // erase current user input
                int previousCommandLength = command.length();
                for (int i=0; i < previousCommandLength; i++) {
                    char *temp = "\b \b";
                    write(STDOUT_FILENO, temp, strlen(temp));
                }
                if (counter == -1) {
                    command = "";
                }
                else {
                    // write requested historical command
                    command = history[counter];
                    char *commandCString = (char *)command.c_str();
                    write(STDOUT_FILENO, commandCString, strlen(commandCString));
                }
            }
            
        }
    }
}

/*int main() {
	struct termios SavedTermAttributes;
	SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);

	char nextChar;
    string command;

    int counter = -1; // tracks how far back in history user is // -1 represents current prompt
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
                            executeArrows(history, command, counter);
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
        counter = -1;
	}
    
    for (int i=0; i < history.size(); i++) {
        cout << history[i] << endl;
    }
    
	ResetCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
    
        return 1;
} */

int main() {
    string mystring;
    vector<vector<char *> > parameters;
    cin >> mystring;
    //mystring = "";
    parameters = checkAdditionalParameters(mystring);
    
    
    for (int i=0; i < parameters.size(); i++) {
        for (int j=0; j < parameters[i].size(); j++) {
            cout << "parameters[" << i << "][" << j << "] is " << parameters[i][j] << endl;
        }
        cout << endl;
    }
    
}


/* TO DO
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

