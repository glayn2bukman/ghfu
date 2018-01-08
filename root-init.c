// thi file creates the basic root accounts shown in SYSTEM-ACCOUNTS.txt
#include "ghfu.h"

int main(int argc, char **argv)
{
    if (argc!=2)
    {
        fprintf(stdout,"Usage: {program} target-directory\n");
        exit(1);
    }

    init("lib",argv[1]); // to be on a safe side, you wanna set argv[1] to "test"

    char *system_accounts[] = {"A1","A2","A3","A4","Sofia Mbabazi","\0"};
    
    Account acc = NULL;

    for (int i=0; strcmp(system_accounts[i],"\0");++i)
        acc = register_member(acc,system_accounts[i],20,false,stdout);

    // now add the other accounts...

    gsave();

    return 0;
}
