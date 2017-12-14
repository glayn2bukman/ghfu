#include "ghfu.h"

int main()
{
	init("lib","test");

    float ACF = 20.0, SAPHIRE=700, RUBY=900, DIAMOND=1100; // account-creation-fee 

    Account b1 = register_member(NULL, "B1",ACF+DIAMOND,false,stdout);

    if(!b1) // no available investments...
        exit(1);

    Account c1 = register_member(b1, "Consumer 1",0,false,stdout);
    create_new_service(c1,23,"rent",110,110*3,false,stdout);
    pay_for_service(c1,23,110*9,false,stdout);

    Account b2 = register_member(b1, "B2",ACF+DIAMOND,false,stdout);
    //buy_property(b2,60*4,false,stdout);

    upgrade_account(c1,b2,ACF+RUBY,false,stdout);

    Account b3 = register_member(b2, "B3",ACF+RUBY,false,stdout);

    Account b4 = register_member(b3, "B4",ACF+DIAMOND,false,stdout);
    //buy_property(b4,60*4,false,stdout);

    Account b5 = register_member(b4, "B5",ACF+SAPHIRE,false,stdout);

    Account b6 = register_member(b5, "B6",ACF+DIAMOND,false,stdout);
    //buy_property(b6,60*4,false,stdout);

    Account b7 = register_member(b6, "B7",ACF,false,stdout);
    //buy_property(b7,60*4,false,stdout);

    Account b8 = register_member(b7, "B8",ACF+DIAMOND,false,stdout);

    Account b9 = register_member(b8, "B9",ACF+DIAMOND,false,stdout);
    Account b10 = register_member(b8, "B10",ACF+RUBY,false,stdout);

    Account b11 = register_member(b9, "B11",ACF+DIAMOND,false,stdout);
    Account b12 = register_member(b10, "B12",ACF+RUBY,false,stdout);

    float monhtly_auto_refill_percentages[13][4][2] = {
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

        {{175, 64},{125, 60},{75, 80},{0, 0}},        

    };

    // now compute and award all monthy bonuses n commissions to all members for a full year
    //for(int i=0; i<13;++i) 
        // monthly_operations(monhtly_auto_refill_percentages[i], stdout);
    //{
    //    update_monthly_auto_refill_percentages(monhtly_auto_refill_percentages[i], "lib", "test");
    //    monthly_operations(stdout);
    //}


    structure_details(b1);
    structure_details(b2);
    structure_details(c1);
    dump_structure_details(b1->id, "test/test.json");

    gsave();

    dump_structure_details(c1->id, "test/test.json");

    printf("\nsearched_investments? %d\n",search_investments(0,0,0,0,"all","test/invs"));
    printf("\nsearched_services? %d\n",search_services("all","all",0,1,1,1,"test/services"));
    
    /*
    ID c1 = register_new_consumer("consumer 1", "test/consumer");
    Consumer c =  get_consumer_by_id(c1);
    printf("c is NULL: %s\n",NULL==c ? "true" : "false");
    printf("names: %s\ntime: %ld", c->names, c->date);
    */
            
    return 0;
}
