#include "ghfu.h"

int main()
{
	init();

    /* create a simple brocker */
    Account brocker1 = register_member(NULL, "Brocker 1",ACCOUNT_CREATION_FEE+ANNUAL_SUBSCRIPTION_FEE);

    /* now, let brocker bring on a new investor */
    Account investor1 = register_member(brocker1, "Investor 1",
        ACCOUNT_CREATION_FEE+ANNUAL_SUBSCRIPTION_FEE+OPERATIONS_FEE+MINIMUM_INVESTMENT
    );

    /* let brocker add second investor */
    Account investor2 = register_member(brocker1, "Investor 2",
        ACCOUNT_CREATION_FEE+ANNUAL_SUBSCRIPTION_FEE+OPERATIONS_FEE+(MINIMUM_INVESTMENT*3)
    );

    /* let first investor add third investor */
    Account investor3 = register_member(investor1, "Investor 3",
        ACCOUNT_CREATION_FEE+ANNUAL_SUBSCRIPTION_FEE+OPERATIONS_FEE+(MINIMUM_INVESTMENT*2)
    );

    /* let third investor add fourth investor */
    Account investor4 = register_member(investor3, "Investor 4",
        ACCOUNT_CREATION_FEE+ANNUAL_SUBSCRIPTION_FEE+OPERATIONS_FEE+(MINIMUM_INVESTMENT*1.4)
    );

    /* let second investor add fifth investor */
    Account investor5 = register_member(investor2, "Investor 5",
        ACCOUNT_CREATION_FEE+ANNUAL_SUBSCRIPTION_FEE+OPERATIONS_FEE+(MINIMUM_INVESTMENT*2.1)
    );


    invest_money(brocker1, 40+10+180+1450, "First Package",2,true);
    invest_money(brocker1, 40+10+180+1360, "Second Package",2,true);
    invest_money(brocker1, 40+10+180+980, "Third Package",2,true);

    structure_details(NULL);

    return 0;
}
