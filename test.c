
#include <stdio.h>
#include <string.h>

// int main()
// {
//     float current = 3.3333 ;
//     char str[50] ;
//     sprintf(str,"%f",current) ;
//     printf("%s\n", str) ;
//     printf("%d" , strlen(str)) ;
//     return 0 ;
// }

void printthis(char *string)
{
    printf("%s" , string ) ;
}

int main()
{
    char str[] = "Hello World! " ;
    printthis(str) ;
    return 0 ;
}