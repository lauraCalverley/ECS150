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
#include <cctype>

using namespace std;

char * trim(char *s) {
    char *result;
    int length = strlen(s);
    int j = 0;


    for(int i=0; i < length; i++) {
        if (s[i] != ' ') {
            result[j] = s[i];
            j++;
        }
    }
    result[j] = '\0';
    return result;
}

int main() {
    char *temp = "   hello   ";
    trim(temp);
    //cout << trim("   hello   ") << endl;   
}
