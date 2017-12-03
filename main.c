#include "ghfu.h"

int main()
{
	init("lib","test");

 
    if(HEAD->next!=NULL) // found daved structure, just load that and leave
    {
        structure_details(get_account_by_id(1));
        fprintf(stdout, "\nLOADED SAVED STRUCTURE\n");
        return 0;
    }

    fprintf(stdout, "\nNO SAVED STRUCTURE FOUND...system dropping to virginity mode\n");
    fprintf(stdout, "press enter to continue...");
    char c;
    fscanf(stdin, "%c",&c);

    /* create a simple brocker */
    Account brocker1 = register_member(NULL, "Brocker 1",12,false,stdout);

    /* now, let brocker bring on a new investor */
    Account investor1 = register_member(brocker1, "Investor 1",
        12+700,false,stdout
    );

    /* let brocker add second investor */
    Account investor2 = register_member(brocker1, "Investor 2",
        12+900,false,stdout
    );


    /* let first investor add third investor */
    Account investor3 = register_member(investor1, "Investor 3",
        12+1100,false,stdout
    );

    /* let third investor add fourth investor */
    Account investor4 = register_member(investor3, "Investor 4",
        12,false,stdout
    );

    /* let second investor add fifth investor */
    Account investor5 = register_member(investor2, "Investor 5",
        12+1100,false,stdout
    );

    /* let brocker add other invetors to make third leg */
    Account investor6 = register_member(brocker1, "Investor 6",
        12+700,false,stdout
    );
    Account investor7 = register_member(brocker1, "Investor 7",
        12+900,false,stdout
    ); /* not supposed to be added...investment scheme will complain */


    invest_money(investor1, 900, true,false,stdout);
    invest_money(brocker1, 700, true,false,stdout);
    invest_money(brocker1, 1100, true,false,stdout);
    
    buy_property(investor2, 360, false,stdout);


    float monhtly_auto_refill_percentages[12][4][2] = {
        /* all arrays MUST be in descending rank order and MUST end with the {0,0} terminating condition */
        {{175, 64},{125, 60},{75, 80},{0, 0}},        

        {{175, 60},{125, 65},{75, 92},{0, 0}},        

        {{175, 43},{125, 51},{75, 76},{0, 0}},        

        {{175, 60},{125, 65},{75, 95},{0, 0}},     

        {{175, 78},{125, 69},{75, 98},{0, 0}},        

        {{175, 67},{125, 63},{75, 100},{0, 0}},        

        {{175, 80},{125, 69},{75, 400},{0, 0}},        

        {{175, 58},{125, 42},{75, 960},{0, 0}},        

        {{175, 53},{125, 65},{75, 97},{0, 0}},        

        {{175, 49},{125, 65},{75, 50},{0, 0}},        

        {{175, 67},{125, 58},{75, 51},{0, 0}},        

        {{175, 62},{125, 66},{75, 65},{0, 0}},        

    };

    /* now compute and award all monthy bonuses n commissions to all members for a full year*/
    for(int i=0; i<12;++i) 
        // monthly_operations(monhtly_auto_refill_percentages[i], stdout);
    {
        update_monthly_auto_refill_percentages(monhtly_auto_refill_percentages[i], "lib", "test");
        monthly_operations(stdout);
    }
        
    //printf("\nSYSTEM FLOAT = $%.2f, TOTAL_COMMISSIONS = $%.2f\n", SYSTEM_FLOAT, CUMULATIVE_COMMISSIONS);
    
    redeem_points(brocker1,1.2,false,stdout);
    redeem_points(brocker1,2.8,false,stdout);
    redeem_points(brocker1,0.76,false,stdout);
    
    structure_details(brocker1);

    printf("\n");
    printf("dumped <%s's> data to json? %s\n", brocker1->names, dump_structure_details(brocker1->id, "test/main-brocker1.json") ? "yes" : "no");
    printf("saved constants? %s\n", dump_constants("lib","test")?"yes":"no");
    printf("loaded constants? %s\n", load_constants("lib","test")?"yes":"no");
    printf("structure saved? %s\n", save_structure("lib","test")?"yes":"no");

    /* if load_structure doesnt do garbage collection properly, 
       you should get a whole host of memory-leaks*/
    //printf("structure loaded? %s\n", load_structure("lib","test")?"yes":"no");

    /* re-create the simple brocker */
    brocker1 = register_member(NULL, "Brocker 1",12+900,false,stdout);
    structure_details(brocker1);
    
    printf("updated monthly-auto-refill %%ges? %s\n",
        update_monthly_auto_refill_percentages(monhtly_auto_refill_percentages[3],"lib","test") ?
        "yes" : "no");
    
    return 0;
}
