/*
    Project name: GHFU (GoodHomesForYou)
    Code name: shadow wallet
    Logic: all explained in the proposal document (see bukman or richard about this)
    Owner: JERM Technology
    License: Private; all rights reserved by JERM technology
*/

/* library inclusion */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

#include "data.h"

/* type declarations */
typedef unsigned long ID;
typedef float Amount;
typedef char *String;

typedef unsigned int bool;
enum {false, true};

typedef struct commission
{
    String reason;
    Amount amount;

    struct commission *next;
    struct commission *prev;

} *Commission;

typedef struct investment
{
    time_t date;
    Amount amount; /* this amount is in points... */

    String package;
    ID package_id; /* 0 indicates a mere investment. otherwise, pkg ids are got from the front-end dev's side */

    Amount returns;
    unsigned int months_returned; /* 0-12, stop giving returns after this value==12 */
    unsigned int days_to_next_return; /* will make the process of identifying which investment to get a 
                                         monthly return easy(the operation is peformed everyday!) */

    struct investment *next;
    struct investment *prev;
} *Investment;

typedef struct account_pointer
{
    void *account;

    struct account_pointer *next;
    struct account_pointer *prev;

} *AccountPointer;

typedef struct account
{
    String names;
    Amount pv; /* Personal Volume (the backbone of all commissions and bonuses) */
    Amount available_balance;
    Amount total_returns;

    struct account *uplink;
    AccountPointer children;
    AccountPointer last_child;

    Commission commissions;
    Commission last_commission;

    Amount leg_volumes[3];
    Amount TVC_level; /* team-volume-commission level. 
                         always subtract this value from the lower leg to compute the TVC.
                         This way, we accomplish the "leveling off" of TVC legs everytime
                         TVCs are calculated
                      */

    Investment investments;
    Investment last_investment;

} *Account;

Amount SYSTEM_FLOAT, CUMULATIVE_COMMISSIONS, COMMISSIONS;
ID ACTIVE_ACCOUNTS;

AccountPointer HEAD, TAIL;

/* function prototypes */
void memerror();
void init();
void increment_pv(Account account, const Amount points);
void award_commission(Account account, const Amount points, const String commission_type, String reason);
bool invest_money(Account account, const Amount amount, const String package, const ID package_id, const bool update_system_float);
Account register_member(Account uplink, String names, Amount amount);

void buy_property(Account IB_account, const Amount amount, const bool member, const String buyer_names);
void auto_refill(Account account);

void show_commissions(const Account account);
void show_leg_volumes(const Account account);
void show_investments(const Account account);
void show_direct_children(const Account account);
void structure_details(const Account account); 

void length_of_all_strings(String strings[], int *length);
void join_strings(char buff[], const String strings[]);

/* function definitions */

void memerror()
{
    printf("%s\n",ERRORS[0]);

    /* dump all data to file here (dont overwrite files, always create new files everytime you dump)*/

    exit(1); /* is exiting the right choice? if so, please first dump all data to file*/
}

void init()
{
    HEAD = (AccountPointer)malloc(sizeof(struct account_pointer));

    if(HEAD==NULL) memerror();

    HEAD->account = NULL;
    HEAD->next = NULL;
    HEAD->prev = NULL;

    TAIL = HEAD;
    
    /* these data variables shall be reset when reading data from file */
    SYSTEM_FLOAT=0; CUMULATIVE_COMMISSIONS=0; COMMISSIONS=0;
    ACTIVE_ACCOUNTS=0;
}

void increment_pv(Account account, const Amount points)
{
    if(account==NULL){printf("%s\n",ERRORS[7]); return;}
    account->pv += points;

    /* now update the appropriate leg CV for all uplinks until ROOT(uplink==NULL) in the tree of this account 
       this dramatically reduces the time needed to calculate the TVCs and all other TEAM-volume related 
       calculations as the values will be readily available
    */

    Account acc = account;
    
    while(acc!=NULL)
    {
        if(acc->uplink!=NULL)
        {
            if(acc==acc->uplink->children->account)
                {acc->uplink->leg_volumes[0] += points;}
            else if(acc==acc->uplink->children->next->account)
                {acc->uplink->leg_volumes[1] += points;}
            else {acc->uplink->leg_volumes[2] += points;}
        }
        acc = acc->uplink;
    }
    
}

void award_commission(Account account, Amount points, String commission_type, String reason)
{
    if(!points){printf("%s\n",ERRORS[8]); return;}
    if(account==NULL) {printf("could not award commissions for NULL account!\n"); return;}
    
    Commission new_commission = (Commission)malloc(sizeof(struct commission));
    if(new_commission==NULL) memerror();

    new_commission->reason = NULL;
    new_commission->amount = 0;

    new_commission->next = NULL;
    new_commission->prev = NULL;

    Amount p_comm=0;
    char points_str[16];
    int buff_length;
    
    if(!strcmp(commission_type,"FSB"))
    {
        /* fast start bonus (whenver a new member joins)*/
        int i=0;
        for(; FSB[i][1]; ++i)
        {   
            if(account->pv < FSB[i+1][0]) break;
        }

        i = FSB[i][1] ? i : --i; /* could have brocken up from the {0,0} terminating array*/

        p_comm = (FSB[i][1]/100)*points;
        
        new_commission->amount = p_comm;
        account->available_balance += p_comm;
        account->total_returns += p_comm;
        /* increment_pv(account,points); THIS IS CATERED FOR WHEN invest_money IS CALLED FROM register_member*/
        
        if(account->commissions==NULL)
        {
            account->commissions = new_commission;
            account->last_commission = account->commissions;
        }
        else
        {
            new_commission->prev = account->last_commission;
            account->last_commission->next = new_commission;
            account->last_commission = new_commission;
        }
    }
    else if(!strcmp(commission_type,"IBC"))
    {
        /* independent brocker commission (whenever a package is bought) */
        int i=0;
        for(; IBC[i][1]; ++i)
        {
            if(account->pv < IBC[i+1][0]) break;
        }

        i = IBC[i][1] ? i : --i; /* could have brocken up from the {0,0} terminating array*/
        
        p_comm = (IBC[i][1]/100)*points;
        
        new_commission->amount = p_comm;
        account->available_balance += p_comm;
        account->total_returns += p_comm;
        increment_pv(account,points);
        
        if(account->commissions==NULL)
        {
            account->commissions = new_commission;
            account->last_commission = account->commissions;
        }
        else
        {
            new_commission->prev = account->last_commission;
            account->last_commission->next = new_commission;
            account->last_commission = new_commission;
        }
    }
    else if(!strcmp(commission_type, "TBB"))
    {
        /* team-building-bonus (whenever a new member joins)*/
        
        free(new_commission); /* you dont want memory leaks, trust me */
        
        if(account->uplink==NULL) return;
        int generation = 1;
        Account uplink = account->uplink->uplink;
        
        Commission last_commission;
        
        while((generation<10) && (uplink!=NULL))
        /* the 2 conditions are arranged in that order for a reason; when we have a sufficiently large 
           structure, its mch more likely that we shall reach the generation<10 barrier of TBBs before
           we reach the root node for that given lineage!
        */
        {
            int i=0;
            for(; TBB[i][1]; ++i)
            {
                if(account->pv < TBB[i+1][0]) break;
            }
            
            i = TBB[i][1] ? i : --i; /* could have brocken up from the {0,0} terminating array*/
            
            p_comm = (TBB[i][generation]/100)*points;
            
            if(p_comm)
            {
                last_commission = uplink->last_commission;

                uplink->last_commission = (Commission)malloc(sizeof(struct commission));
                if(uplink->last_commission==NULL) memerror();

                uplink->last_commission->reason = NULL;
                uplink->last_commission->amount = p_comm;

                uplink->last_commission->next = NULL;
                uplink->last_commission->prev = last_commission;

                uplink->available_balance += p_comm;
                uplink->total_returns += p_comm;
                
                /* generate custom reason for the commission */
                sprintf(points_str," ($%.2f)",p_comm);
                String reason_strings[] = {"TBB from ", account->names, points_str,"\0"};
                length_of_all_strings(reason_strings, &buff_length);

                /* create a new array to hold this commission reason (you cant just assign a local char[] to
                   new_commission->reason...basically U DONT WANNA ASSIGN TO A LOCAL POINTER BCOZ ONCE D FUCTION
                   EXITS, THE POINTER WILL BE ANONYMOUS!)*/
                uplink->last_commission->reason = (String)malloc(sizeof(char)*(buff_length+1));
                if(uplink->last_commission->reason==NULL) memerror();
                join_strings(uplink->last_commission->reason, reason_strings);
                
                if(uplink->commissions==NULL){ uplink->commissions = uplink->last_commission; }
                else
                {
                    uplink->last_commission->prev = last_commission;
                    last_commission->next = uplink->last_commission;
                }


                CUMULATIVE_COMMISSIONS += p_comm;
                COMMISSIONS += p_comm;
            }
            
            uplink = uplink->uplink;
            ++generation;
        }
        
        p_comm = 0; /* without this, we have a memory leak due to the if(p_comm) below */
    }

    if(p_comm)
    {
        sprintf(points_str," ($%.2f)",p_comm);
        String reason_strings[] = {reason, points_str, "\0"};
        length_of_all_strings(reason_strings, &buff_length);

        /* create a new array to hold this commission reason (you cant just assign a local char[] to
           new_commission->reason...basically U DONT WANNA ASSIGN TO A LOCAL POINTER BCOZ ONCE D FUCTION
           EXITS, THE POINTER WILL BE ANONYMOUS!)*/
        new_commission->reason = (String)malloc(sizeof(char)*(buff_length+1));
        if(new_commission->reason==NULL) memerror();
        join_strings(new_commission->reason,reason_strings);

        CUMULATIVE_COMMISSIONS += p_comm;
        COMMISSIONS += p_comm;
    }
}

bool invest_money(Account account, Amount amount, String package, ID package_id, bool update_system_float)
{
    if(account==NULL) {printf("could not create investment for NULL account!\n"); return false;}

    amount -= OPERATIONS_FEE;
    if(amount<0){ printf("failed to invest for <%s>; %s\n",account->names,ERRORS[4]); return false; }
    if(!amount){ printf("failed to invst for <%s>; %s\n",account->names,ERRORS[5]); return false; }
    if(amount<MINIMUM_INVESTMENT){ printf("failed to invest for <%s>; %s\n",account->names,ERRORS[6]); return false; }
    if(amount>MAXIMUM_INVESTMENT){ printf("failed to invest for <%s>; %s\n",account->names,ERRORS[9]); return false; }

    Investment new_investment = (Investment)malloc(sizeof(struct investment));
    
    if(new_investment==NULL) memerror();

    Amount points = amount*POINT_FACTOR;
    
    time(&(new_investment->date));

    new_investment->amount = points;
    new_investment->package = package;
    new_investment->package_id = package_id;

    new_investment->returns = 0;
    new_investment->months_returned = 0;
    new_investment->days_to_next_return = 30;

    new_investment->next = NULL;
    new_investment->prev = NULL;

    if(account->investments==NULL)
    {
        account->investments = new_investment;
        account->last_investment = new_investment;
    }
    else
    {
        account->last_investment->next = new_investment;
        new_investment->prev = account->last_investment;
        account->last_investment = new_investment;
    }

    if(update_system_float) SYSTEM_FLOAT += amount;

    increment_pv(account, points);

    return true; /* leave this out and you have memory leaks when invest money is called from regiter_member*/
}

Account register_member(Account uplink, String names, Amount amount)
{
    /* if this function returns NULL, DONT USE RESULT (it indicates an error, more like malloc)*/
    if(amount<(ACCOUNT_CREATION_FEE + ANNUAL_SUBSCRIPTION_FEE))
        { printf("failed to add <%s>; ERROR: %s\n",names,ERRORS[1]); return NULL; }
    
    Account new_account = (Account)malloc(sizeof(struct account));
    
    if(new_account==NULL) memerror();

    new_account->names = names;
    new_account->pv = 0;
    new_account->available_balance = 0;
    new_account->total_returns = 0;
    new_account->uplink = uplink;
    new_account->children = NULL;
    new_account->last_child = NULL;
    new_account->commissions = NULL;
    new_account->last_commission = NULL;

    new_account->leg_volumes[0] = 0.0;
    new_account->leg_volumes[1] = 0.0;
    new_account->leg_volumes[2] = 0.0;

    new_account->TVC_level = 0;

    new_account->investments = NULL;
    new_account->last_investment = NULL;

    /* attempt to invest money (if amount>(ACCOUNT_CREATION_FEE + ANNUAL_SUBSCRIPTION_FEE))*/
    Amount _amount = amount;
    amount = amount-(ACCOUNT_CREATION_FEE + ANNUAL_SUBSCRIPTION_FEE);
    
    /* turn investable amount to points */
    Amount points=amount*POINT_FACTOR;

    
    /* add new_account to uplink's children and then give uplink FSB */
    AccountPointer ap1 = (AccountPointer)malloc(sizeof(struct account_pointer));

    if(uplink!=NULL)
    {
        /* add the new account to uplink's children */
        AccountPointer ap = ap1;
        if(ap==NULL)
        {
            free(new_account); /* you dont want memory leaks, trust me */
            memerror();
        }
        
        ap->account = new_account;
        ap->next = NULL;

        if(uplink->children==NULL) { ap->prev = NULL; uplink->children = ap;}
        else { ap->prev = uplink->last_child; uplink->last_child->next = ap;}

        uplink->last_child = ap;
        
        if(points)
        {
            /* create reason for the commission and award the commission */
            String reason_strings[] = {"FSB from ",names,"\0"};
            int buff_length;
            length_of_all_strings(reason_strings, &buff_length);
            char buff[buff_length+1];
            join_strings(buff,reason_strings);

            award_commission(uplink, points, "FSB",buff);
        }
    }
    else {free(ap1); /* you dont want memory leaks, trust me */ }

    if(points) award_commission(new_account, points,"TBB","");

    /* add new node to linear structure of all nodes */
    AccountPointer ap = (AccountPointer)malloc(sizeof(struct account_pointer));
    if(ap==NULL)
    {
         /* you dont want memory leaks, trust me */
        if(uplink!=NULL) free(ap1);
        free(new_account);
        
        memerror();
    }

    ap->account = new_account;
    ap->next = NULL;

    if(HEAD->next==NULL) { ap->prev = HEAD; HEAD->next = ap; }
    else { ap->prev = TAIL; TAIL->next = ap; }

    TAIL = ap;

    /* attempt to invest money (after declaring that new_member is a child of uplink; this is important
       because invest_money calls incrememt_pv which needs to call children of uplinks)*/
    if(points)
    {
        if(!invest_money(new_account, amount, "Investment", 0, false))
        {
            free(new_account); /* you dont want memory leaks, trust me */
            return NULL;
        }
    }
    
    /* update system float */
    SYSTEM_FLOAT += _amount;

    return new_account;
}

void buy_property(Account IB_account, Amount amount, bool member, String buyer_names)
{
    /* if member(IV or registered investor), get IBC and those points otherwise just add this amount to 
       the system total float */
    Amount points = amount*POINT_FACTOR;
    
    if(!member){SYSTEM_FLOAT += amount; return;}
    
    if(IB_account==NULL){printf("%s\n", ERRORS[3]); return;}

    /* create reason for the commission */
    String reason_strings[] = {"IBC from ",buyer_names,"\0"};
    int buff_length;
    length_of_all_strings(reason_strings, &buff_length);
    char reason[buff_length+1];
    join_strings(reason,reason_strings);

    award_commission(IB_account, points, "IBC", reason);

    SYSTEM_FLOAT += amount;
}

void auto_refill(Account account)
{
    /* if account==NULL, calculate autp-refills for all investments in the system otherwise, 
       calculate auto-refill for only that account's investments 
    */
    if(account==NULL)
    {
        /* for account in accounts (starting from HEAD->next)*/
    }
}

void show_commissions(Account account)
{
    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */
    Commission c;
    
    if (account!=NULL)
    {        
        printf("\n  %s's COMMISSIONS & BONUSES\n",account->names);
        c = account->commissions;
        if(c==NULL) printf("    None\n");

        int i = 1;
        while(c!=NULL){printf("    %d) %s\n",i, c->reason); c=c->next; ++i;}        
        
        return;
    }

    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = acc_p==NULL ? acc_p : acc_p->next;

    while(acc_p!=NULL)
    {
        acc = acc_p->account;

        printf("\n  %s's COMMISSIONS & BONUSES\n",acc->names);
        c = acc->commissions;
        if(c==NULL) printf("    None\n");

        int i = 1;
        while(c!=NULL){printf("    %d) %s\n",i, c->reason); c=c->next; ++i;}

        acc_p = acc_p->next;
    }
}

void show_leg_volumes(Account account)
{
    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    if(account!=NULL)
    {
        printf("%s leg volumes: [%.0f, %.0f, %.0f]\n",account->names, 
            account->leg_volumes[0], account->leg_volumes[1], account->leg_volumes[2]);
        return;
    }

    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = acc_p==NULL ? acc_p : acc_p->next;

    while(acc_p!=NULL)
    {
        acc = acc_p->account;

        printf("%s leg volumes: [%.0f, %.0f, %.0f]\n",acc->names, 
            acc->leg_volumes[0], acc->leg_volumes[1], acc->leg_volumes[2]);

        acc_p = acc_p->next;
    }
}

void show_investments(const Account account)
{
    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    Investment inv;
    if(account!=NULL)
    {
        printf("\n  %s's INVESTMENTS\n",account->names);

        inv = account->investments;

        if(inv==NULL) printf("    None\n");
        
        while(inv!=NULL)
        {
            printf("    Date: %ld\n",inv->date);
            printf("    Weight: %.2f points\n",inv->amount);
            printf("    Package: %s\n",inv->package);
            printf("    Months returned: %d\n",inv->months_returned);
            printf("    Days to next return: %d\n",inv->days_to_next_return);
            
            inv = inv->next;
            
            if(inv!=NULL) printf("    --------\n");
        }
        return;
    }
    
    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = acc_p==NULL ? acc_p : acc_p->next;
    
    while(acc_p!=NULL)
    {
        acc = acc_p->account;

        printf("\n  %s's INVESTMENTS\n",acc->names);

        inv = account->investments;

        if(inv==NULL) printf("    None\n");

        while(inv!=NULL)
        {
            printf("    Date: %ld\n",inv->date);
            printf("    Weight: %.2f points\n",inv->amount);
            printf("    Package: %s\n",inv->package);
            printf("    Months returned: %d\n",inv->months_returned);
            printf("    Days to next return: %d\n",inv->days_to_next_return);
            
            inv = inv->next;
            
            if(inv!=NULL) printf("    --------\n");
        }

        acc_p = acc_p->next;
    }
}

void show_direct_children(const Account account)
{
    AccountPointer acc_p, acc_p_child;
    Account acc, acc_child;

    if(account!=NULL)
    {
        printf("\n  %s's DIRECT DESCENDANTS\n",account->names);

        acc_p = account->children;
        
        if(acc_p==NULL) printf("    None\n");

        int i=1;
    
        while(acc_p!=NULL)
        {
            acc = acc_p->account;
            printf("    %d) %s\n",i, acc->names); 

            acc_p=acc_p->next; 
            ++i;
        }

        return;
    }
    
    acc_p = HEAD;
    acc_p = acc_p==NULL ? acc_p : acc_p->next;

    while(acc_p!=NULL)
    {
        acc = acc_p->account;
        printf("\n  %s's DIRECT DESCENDANTS\n",acc->names);

        acc_p_child = acc->children;
        
        if(acc_p_child==NULL) printf("    None\n");

        int i=1;
    
        while(acc_p_child!=NULL)
        {
            acc_child = acc_p_child->account;
            printf("    %d) %s\n",i, acc_child->names); 

            acc_p_child=acc_p_child->next; 
            ++i;
        }

        acc_p = acc_p->next;
    }
}

void structure_details(const Account account)
{
    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    /* atempt to provide all details about all members in the system */
    if(account!=NULL)
    {
        printf("\n%s\n",account->names);
        printf("  Uplink: %s\n", account->uplink==NULL ? "ROOT" : account->uplink->names);
        printf("  PV = %.2lf points\n",account->pv);
        printf("  Total Returns = $%.2lf\n",account->total_returns);
        printf("  Available Balance = $%.2lf\n",account->available_balance);
        printf("  Leg Volumes = (%.2lf, %.2lf, %.2lf)\n",
            account->leg_volumes[0],account->leg_volumes[1],account->leg_volumes[2]);
        
        show_investments(account);
        show_commissions(account);
        show_direct_children(account);
        
        return;
    }
    
    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = acc_p==NULL ? acc_p : acc_p->next;
    
    while(acc_p!=NULL)
    {
        acc = acc_p->account;

        printf("\n%s\n",acc->names);
        printf("  Uplink: %s\n", acc->uplink==NULL ? "ROOT" : acc->uplink->names);
        printf("  PV = %.2lf points\n",acc->pv);
        printf("  Total Returns = $%.2lf\n",acc->total_returns);
        printf("  Available Balance = $%.2lf\n",acc->available_balance);
        printf("  Leg Volumes = (%.2lf, %.2lf, %.2lf)\n",
            acc->leg_volumes[0],acc->leg_volumes[1],acc->leg_volumes[2]);
        
        show_investments(acc);
        show_commissions(acc);
        show_direct_children(acc);

        acc_p = acc_p->next;
    }
    
}

void length_of_all_strings(String strings[], int *length)
{
    *length=0;
    for(int i=0; strcmp(strings[i],"\0"); ++i) *length += strlen(strings[i]);    
}

void join_strings(char buff[], const String strings[])
{
    /* be absolutely sure that sizeof==sum(sizeof strings) + 1 !!!!!!!!!!!!!!!!!!!!! */
    int i=0, index=0, len=0;

    while(true)
    {
        if(!strcmp(strings[i], "\0")) {buff[index]='\0'; break;}
        
        len = strlen(strings[i]);
        memcpy( &(buff[index]), strings[i], len);
        index += len;
        ++i;
    }
}
