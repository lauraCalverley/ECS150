#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>

#include <string>
#include <vector>
#include <deque>
#include <cstring>
#include <iostream>

using namespace std;

void SetNonCanonicalMode(int fd, struct termios *savedattributes);
void ResetCanonicalMode(int fd, struct termios *savedattributes);
void printNewLine();
string executeBackspace(string command);
void executeInvalidCommand(string command);
void executePwd(vector<vector<char *> > parsedInput);
void getPerms(string &perms, struct dirent* dp, struct stat statbuf, string path);
void executeLs(vector<vector<char*> > parsedInput);
void executeFf(vector<vector<char*> > parsedInput, char* directory);
void executeCd(vector<vector<char *> > parsedInput);
void printShellPrompt();
void parseCommand(string command, vector<vector<char *> > &parameters);
void directCommand(string command);
void executeArrows(deque<string> history, string &command, int &counter);
    
void SetNonCanonicalMode(int fd, struct termios *savedattributes){ // Source: Nitta noncanonmode.c
    struct termios TermAttributes;
    
    // Make sure stdin is a terminal. 
    if(!isatty(fd)){
        char *errorMessage = "Not a terminal.";
        write(STDERR_FILENO, errorMessage, strlen(errorMessage));
        printNewLine();
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

void ResetCanonicalMode(int fd, struct termios *savedattributes){ // Source: Nitta's noncanonmode.c
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

void executeInvalidCommand(string command) {
    printNewLine();
    char *temp = "Failed to execute ";
    write(STDOUT_FILENO, temp, strlen(temp));
    write(STDOUT_FILENO, command.c_str(), strlen(command.c_str()));
    printNewLine();
}

void executePwd(vector<vector<char *> > parsedInput) { // to be forked? yes
    
    char *directoryName = NULL;
    directoryName = getcwd(directoryName, 0);
    printNewLine();

    if (!parsedInput[3].empty()) { //redirect output
        // open files for output
        //int flags = O_CREAT | S_IRUSR | S_IWUSR | O_RDWR;
        int outputFD;
        
        for (int i=0; i < parsedInput[3].size(); i++) {
            // create a file for each output vector entry
            outputFD = open(parsedInput[3][i], O_CREAT, 0777); // returns the new file descriptor // Source: http://stackoverflow.com/questions/2245193/why-does-open-create-my-file-with-the-wrong-permissions
            close(outputFD);
        }
            
        // write to final output vector entry file
        //flags = S_IRUSR | S_IWUSR | O_RDWR;
        outputFD = open(parsedInput[3][(parsedInput[3].size() - 1)], O_RDWR, 0777); // returns the new file descriptor
        write(outputFD, directoryName, strlen(directoryName));
    }
    
    else {
        write(STDOUT_FILENO, directoryName, strlen(directoryName));
        printNewLine();
    }
}

void getPerms(string &perms, struct dirent* dp, struct stat statbuf, string path){
    stat(path.c_str(), &statbuf);
    if(dp->d_type == DT_DIR){
        perms += 'd';
    } else {perms += '-';}
    if(S_IRUSR & statbuf.st_mode){
        perms += 'r';
    } else {perms += '-';}
    if(S_IWUSR & statbuf.st_mode){
        perms += 'w';
    } else {perms += '-';}
    if(S_IXUSR & statbuf.st_mode){
        perms += 'x';
    } else {perms += '-';}
    if(S_IRGRP & statbuf.st_mode){
        perms += 'r';
    } else {perms += '-';}
    if(S_IWGRP & statbuf.st_mode){
        perms += 'w';
    } else {perms += '-';}
    if(S_IXGRP & statbuf.st_mode){
        perms += 'x';
    } else {perms += '-';}
    if(S_IROTH & statbuf.st_mode){
        perms += 'r';
    } else {perms += '-';}
    if(S_IWOTH & statbuf.st_mode){
        perms += 'w';
    } else {perms += '-';}
    if(S_IXOTH & statbuf.st_mode){
        perms += 'x';
    } else {perms += '-';}
}


void executeLs(vector<vector<char*> > parsedInput){
    printNewLine();

    // Source: http://pubs.opengroup.org/onlinepubs/009695399/functions/stat.html
    DIR * dir; 
    struct dirent *dp; 
    struct stat statbuf;
    string filePath;
    string makePerms = "";
    const char* perms; //permissions

    char *directoryPath = NULL;
    directoryPath = getcwd(directoryPath, 0);
    
    if(parsedInput[0].size() == 1){ //no parameters
        dir = opendir("."); //open the current directory
        while((dp = readdir(dir)) != NULL){ //loop through directory
            filePath = (string)directoryPath + "/" + (string)(dp->d_name); // build absolute file path
            getPerms(makePerms, dp, statbuf, filePath);
            makePerms = makePerms + " "; // insert space character between permissions and file name
            perms = makePerms.c_str();
            write(STDOUT_FILENO, perms, strlen(perms)); 
            write(STDOUT_FILENO, dp->d_name, strlen(dp->d_name)); //Source: http://pubs.opengroup.org/onlinepubs/009695399/functions/readdir.html
            printNewLine();
            makePerms = "";
        }
    }
    else { // 1 parameter case
        dir = opendir(parsedInput[0][1]); //open the desired directory
        char *directoryPath = NULL;
        directoryPath = getcwd(directoryPath, 0);
        while((dp = readdir(dir)) != NULL){
            filePath = (string)directoryPath + "/" + parsedInput[0][1] + "/" + (string)(dp->d_name); // build absolute file path
            getPerms(makePerms, dp, statbuf, filePath);
            makePerms = makePerms + " "; // insert space character between permissions and file name
            perms = makePerms.c_str();
            write(STDOUT_FILENO, perms, strlen(perms));
            write(STDOUT_FILENO, dp->d_name, strlen(dp->d_name)); //Source http://pubs.opengroup.org/onlinepubs/009695399/functions/readdir.html
            printNewLine();
            makePerms = "";
        }

    }
}


void executeFf(vector<vector<char*> > parsedInput, char* directory){
    if (parsedInput[0].size() == 1) {
        char *errorMessage = "ff command requires a filename!";
        printNewLine();
        write(STDOUT_FILENO, errorMessage, strlen(errorMessage));
    }
    else {
    
	    DIR * dir;
	    struct dirent *dp;
	    dir = opendir(directory);
	    string path = directory;
	    const char* newDirectory;
	    path += '/';
	    const char* output;
	    while((dp = readdir(dir)) != NULL){
            if(dp->d_type == DT_DIR && strcmp(dp->d_name,".") && strcmp(dp->d_name, "..")){
                path += dp->d_name;
                newDirectory = path.c_str();
                executeFf(parsedInput, (char*)newDirectory);
                path.erase(path.size() - strlen(dp->d_name), path.npos);
            }
            else if(!strcmp(parsedInput[0][1], dp->d_name)){
                path += dp->d_name;
                output = path.c_str();
                printNewLine();
                write(STDOUT_FILENO, (char*)output, strlen(output));
                path.erase(path.size() - strlen(dp->d_name), path.npos);
            }
	    }
    }
}


void executeCd(vector<vector<char *> > parsedInput) {
    printNewLine();
    char const *directoryName;

    if (parsedInput[0].size() == 1) {
        directoryName = getenv("HOME");
    }
    else {
        
        directoryName = parsedInput[0][1];
    }
        
    int success = chdir(directoryName);
        
    if (success == -1) {
        char *errorMessage = "Error changing directory.";
        write(STDOUT_FILENO, errorMessage, strlen(errorMessage));
        printNewLine();
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

void parseCommand(string command, vector<vector<char *> > &parameters) {
    vector<char *> tokens;
    vector<char *> commandVector, pipeVector, inputVector, outputVector;
    string temp = "";

    char const *commandTemp = command.c_str();
    char commandC[command.length()];
    
    strcpy(commandC, commandTemp);
    
    char *token;
    token = strtok(commandC, " ");
    
    while (token != NULL) {
        tokens.push_back(token);
        token = strtok(NULL, " ");
    }
    
    int i = 0;
    char *currentToken;
    while (i < tokens.size()) {
        if (i < tokens.size()) {
            currentToken = tokens[i];
        }
        if (i == 0) {
            while (strcmp(currentToken, "|") && strcmp(currentToken, "<") && strcmp(currentToken, ">") && (i < tokens.size())) {
                char* c = new char[strlen(currentToken) + 1]; // Source: http://www.cplusplus.com/forum/beginner/16987/
                strcpy(c, currentToken);
                commandVector.push_back(c);
                i++;
                if (i < tokens.size()) {
                    currentToken = tokens[i];
                }
            }
        }
        else if (!strcmp(currentToken, "|")) {
            temp = "";
            i++;
            if (i < tokens.size()) {
                currentToken = tokens[i];
            }
            while (strcmp(currentToken, "|") && strcmp(currentToken, "<") && strcmp(currentToken, ">") && (i < tokens.size())) {
                temp = temp + currentToken + " ";
                i++;
                if (i < tokens.size()) {
                    currentToken = tokens[i];
                }
            }
            temp.erase(temp.end() - 1);

            char* c = new char[temp.length() + 1]; // Source: http://www.cplusplus.com/forum/beginner/16987/
            strcpy(c, (char*) temp.c_str());
            pipeVector.push_back(c);
        }
        else if (!strcmp(currentToken, "<")) {
            i++;
            if (i < tokens.size()) {
                currentToken = tokens[i];
            }
            if (strcmp(currentToken, "|") && strcmp(currentToken, "<") && strcmp(currentToken, ">") && (i < tokens.size())) {
                char* c = new char[strlen(currentToken) + 1]; // Source: http://www.cplusplus.com/forum/beginner/16987/
                strcpy(c, currentToken);
                inputVector.push_back(c);
            }
            else {
                // ERROR
            }
            
            while (strcmp(currentToken, "|") && strcmp(currentToken, "<") && strcmp(currentToken, ">") && (i < tokens.size())) {
                i++;
                if (i < tokens.size()) {
                    currentToken = tokens[i];
                }
            }
        }
        else if (!strcmp(currentToken, ">")) {
            i++;
            if (i < tokens.size()) {
                currentToken = tokens[i];
            }
            if (strcmp(currentToken, "|") && strcmp(currentToken, "<") && strcmp(currentToken, ">") && (i < tokens.size())) {
                char* c = new char[strlen(currentToken) + 1]; // Source: http://www.cplusplus.com/forum/beginner/16987/
                strcpy(c, currentToken);
                outputVector.push_back(c);
            }
            else {
                // ERROR
            }
            
            while (strcmp(currentToken, "|") && strcmp(currentToken, "<") && strcmp(currentToken, ">") && (i < tokens.size())) {
                i++;
                if (i < tokens.size()) {
                    currentToken = tokens[i];
                }
            }
        }
        else {
            // ERROR
        }
    }

    parameters.push_back(commandVector);
    parameters.push_back(pipeVector);
    parameters.push_back(inputVector);
    parameters.push_back(outputVector);
    
}


void directCommand(string command) {
    
    vector<vector<char *> > parsedInput;
    parseCommand(command, parsedInput);
    
    string commandType = parsedInput[0][0];
    
    if (commandType == "cd") {
        executeCd(parsedInput);
    }
    else if (commandType == "ls") {
        executeLs(parsedInput);
    }
    else if (commandType == "pwd") {
        executePwd(parsedInput);
    }
    else if (commandType == "ff") {
        if (parsedInput[0].size() >= 3) { // user gave directory parameter
            char *directory = parsedInput[0][2];
            executeFf(parsedInput, directory);
        }
        else {
            executeFf(parsedInput, ".");
        }
        printNewLine();
    }
    else if (commandType == "exit") {
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
            if (counter == -2) { //update to deal with the case where the user types, then hits UP, then hits DOWN (enter is never pressed)
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

int main() {
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
                        case 0x1B: { //escape character
                            executeArrows(history, command, counter);
                            break;
                        }
                        default: { //input chars
                            write(STDOUT_FILENO, &nextChar, 1);
                            command += nextChar;
                            break;
                        }

                    }
                    read(STDIN_FILENO, &nextChar, 1);

                }
        if (command!="") {
            directCommand(command);
            history.push_front(command);
            if (history.size() > 10) {
                history.pop_back();
            }
            counter = -1;
        }
        else {
            printNewLine();
        }
	}
    
	ResetCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
    
        return 1;
}
