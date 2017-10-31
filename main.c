#include "ghfu.h"

int main()
{
	init();

    /* create a simple brocker */
    Account brocker1 = register_member(NULL, "Brocker 1",50);

    /* now, let brocker bring on a new investor */
    Account investor1 = register_member(brocker1, "Investor 1",
        10+40+180+500
    );

    /* let brocker add second investor */
    Account investor2 = register_member(brocker1, "Investor 2",
        10+40+180+(500*3)
    );


    /* let first investor add third investor */
    Account investor3 = register_member(investor1, "Investor 3",
        10+40+180+(500*2)
    );

    /* let third investor add fourth investor */
    Account investor4 = register_member(investor3, "Investor 4",
        10+40+180+(500*1.4)
    );

    /* let second investor add fifth investor */
    Account investor5 = register_member(investor2, "Investor 5",
        10+40+180+(500*2.1)
    );

    /* let brocker add other invetors to make third leg */
    Account investor6 = register_member(brocker1, "Investor 6",
        10+40+180+(500*1.46)
    );
    Account investor7 = register_member(brocker1, "Investor 7",
        10+40+180+(500*2.46)
    );


    invest_money(brocker1, 40+10+180+1450, "First Package",2,true);
    invest_money(brocker1, 40+10+180+1360, "Second Package",2,true);
    invest_money(brocker1, 40+10+180+980, "Third Package",2,true);


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
    for(int i=0; i<12;++i) monthly_operations(monhtly_auto_refill_percentages[i]);

        
    //structure_details(NULL);
    
    printf("\nSYSTEM FLOAT = $%.2f, TOTAL_COMMISSIONS = $%.2f\n", SYSTEM_FLOAT, CUMULATIVE_COMMISSIONS);
    
    printf("dumped data? %s\n", dump_structure_details(brocker1, "brocker1.json") ? "yes" : "no");
    
    return 0;
}
