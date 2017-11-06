#include "ghfu.h"

int main()
{
	init("lib","test");

/*
    if(HEAD->next!=NULL) // found daved structure, just load that and leave
    {
        structure_details(NULL);
        fprintf(stdout, "\nLOADED SAVED STRUCTURE\n");
        return 0;
    }

    fprintf(stdout, "\nNO SAVED STRUCTURE FOUND...system dropping to virginity mode\n");
    fprintf(stdout, "press enter to continue...");
    char c;
    fscanf(stdin, "%c",&c);
*/
    /* create a simple brocker */
    Account brocker1 = register_member(NULL, "Brocker 1",50,stdout);

    /* now, let brocker bring on a new investor */
    Account investor1 = register_member(brocker1, "Investor 1",
        10+40+180+500,stdout
    );

    /* let brocker add second investor */
    Account investor2 = register_member(brocker1, "Investor 2",
        10+40+180+(500*3),stdout
    );


    /* let first investor add third investor */
    Account investor3 = register_member(investor1, "Investor 3",
        10+40+180+(500*2),stdout
    );

    /* let third investor add fourth investor */
    Account investor4 = register_member(investor3, "Investor 4",
        10+40+180+(500*1.4),stdout
    );

    /* let second investor add fifth investor */
    Account investor5 = register_member(investor2, "Investor 5",
        10+40+180+(500*2.1),stdout
    );

    /* let brocker add other invetors to make third leg */
    Account investor6 = register_member(brocker1, "Investor 6",
        10+40+180+(500*1.46),stdout
    );
    Account investor7 = register_member(brocker1, "Investor 7",
        180,stdout
    ); /* not supposed to be added...investment scheme will complain */


    invest_money(investor1, 40+10+180+1450, "First Package",2,true, stdout);
    invest_money(brocker1, 40+10+180+1360, "Second Package",2,true, stdout);
    invest_money(brocker1, 40+10+180+980, "Third Package",2,true, stdout);
    
    buy_property(investor2, 360, true,"hehe",stdout);


    float monhtly_auto_refill_percentages[12][4][2] = {
        /* all arrays MUST be in descending rank order and MUST end with the {0,0} terminating condition */
        {{375, 64},{250, 60},{125, 49},{0, 0}},        

        {{375, 60},{250, 65},{125, 60},{0, 0}},        

        {{375, 43},{250, 51},{125, 54},{0, 0}},        

        {{375, 60},{250, 65},{125, 60},{0, 0}},     

        {{375, 78},{250, 69},{125, 65},{0, 0}},        

        {{375, 67},{250, 63},{125, 59},{0, 0}},        

        {{375, 80},{250, 69},{125, 65},{0, 0}},        

        {{375, 58},{250, 42},{125, 48},{0, 0}},        

        {{375, 53},{250, 65},{125, 60},{0, 0}},        

        {{375, 49},{250, 65},{125, 52},{0, 0}},        

        {{375, 67},{250, 58},{125, 49},{0, 0}},        

        {{375, 62},{250, 66},{125, 48},{0, 0}},        

    };

    /* now compute and award all monthy bonuses n commissions to all members for a full year*/
    for(int i=0; i<12;++i) monthly_operations(monhtly_auto_refill_percentages[i], stdout);

        
    //printf("\nSYSTEM FLOAT = $%.2f, TOTAL_COMMISSIONS = $%.2f\n", SYSTEM_FLOAT, CUMULATIVE_COMMISSIONS);
    
    
    structure_details(brocker1);

    printf("\n");
    printf("dumped <%s's> data to json? %s\n", brocker1->names, dump_structure_details(brocker1->id, "test/main-brocker1.json") ? "yes" : "no");
    printf("saved constants? %s\n", dump_constants("lib","test")?"yes":"no");
    printf("structure saved? %s\n", save_structure("lib","test")?"yes":"no");

    /* if load_structure doesnt do garbage collection properly, 
       you should get a whole host of memory-leaks*/
    //printf("structure loaded? %s\n", load_structure("lib","test")?"yes":"no");

    /* re-create the simple brocker */
    brocker1 = register_member(NULL, "Brocker 1",50,stdout);
    structure_details(brocker1);
    
    return 0;
}
