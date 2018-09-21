#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char phone[11];
    int code = -2;
    scanf("%10s", phone);
    int err = 0;
    while (scanf("%d", &code) == 1)
    {
        if (code == -1)
        {
            printf("%s\n", phone);
        }
        if (code < -1 || code > 9)
        {
            printf("ERROR\n");
            err = 1;
        }
        if (code >= 0 && code <= 9)
        {
            printf("%c\n", phone[code]);
        }
    }
    return err;
}