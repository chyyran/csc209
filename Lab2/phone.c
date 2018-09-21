#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    char phone[11];
    int code = -2;
    scanf("%10s %d", phone, &code);
    if (code == -1) {
        printf("%s", phone);
        return 0;
    }
    if (code < -1 || code > 9) {
        printf("ERROR");
        return 1;
    }
    if (code >= 0 && code <= 9) 
    {
        printf("%c", phone[code]);
    }
}