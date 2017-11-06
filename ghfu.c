/* export this file as a library with
    $ gcc -shared -fPIC -o libjermGHFU.so ghfu.c
*/

#include "ghfu.h"
#include "data.h"

/* function definitions */

void memerror(FILE *fout)
{
    ghfu_warn(0,fout);

    /* dump all data to file here (dont overwrite files, always create new files everytime you dump)*/

    exit(1); /* is exiting the right choice? if so, please first dump all data to file*/
}

void init(String jermCrypt_path, String save_dir)
{
    /* init should ONLY BE CALLED ONCE, at stystem start */

    unsigned int buff_length;
        
    String data_file_paths[] = {save_dir, "/",DATA_FILE,"\0"};
    
    length_of_all_strings(data_file_paths, &buff_length);
    char data_file_path[buff_length+1];
    join_strings(data_file_path, data_file_paths);

    FILE *fin = fopen(data_file_path,"rb");

    if(fin)
    {
        fclose(fin);
        load_constants(jermCrypt_path, save_dir);
    }
    else
    {
        /* these data variables shall be reset when reading data from file */
        SYSTEM_FLOAT=0; CUMULATIVE_COMMISSIONS=0; COMMISSIONS=0;
        
        ACTIVE_ACCOUNTS=0; /* decremeneted when account is deleted */

        CURRENT_ID = 0; /* only increments, never the opposite */
    }

    HEAD = (AccountPointer)malloc(sizeof(struct account_pointer));

    if(HEAD==NULL) memerror(stdout);

    HEAD->account = NULL;
    HEAD->next = NULL;
    HEAD->prev = NULL;

    TAIL = HEAD;

    String structure_file_paths[] = {save_dir, "/",STRUCTURE_FILE,"\0"};
    
    length_of_all_strings(structure_file_paths, &buff_length);
    char structure_file_path[buff_length+1];
    join_strings(structure_file_path, structure_file_paths);

    fin = fopen(structure_file_path,"rb");

    if(fin) 
    {
        fclose(fin); /* you dont want load_structure to attempt opening this file when its already
                        opened in init
                     */
        load_structure(jermCrypt_path, save_dir);
    }

}

void increment_pv(Account account, const Amount points, FILE *fout)
{
    /* this function ensures that all PV increments anss rank updates in a certain line of sucession
       occur are updated all at one when a PV changes. this in turn means that the web-API that calls
       for information never delays because the data is not calculated/determined at calling time but
       rather is updated and made readily available any time the web-API needs it 
    */
    if(account==NULL){ghfu_warn(7,fout); return;}
    account->pv += points;

    /* now update the appropriate leg CV for all uplinks until ROOT(uplink==NULL) in the tree of this account 
       this dramatically reduces the time needed to calculate the TVCs and all other TEAM-volume related 
       calculations as the values will be readily available when needed

       also, increment ranks for all liable members in that linear tree!
    */

    Account acc = account;
    unsigned int highest_rank, i;
    
    while(acc!=NULL)
    {
        raise_rank(acc, fout);

        highest_rank = acc->highest_leg_ranks[0];
        for(i=1;i<3;++i)
            acc->highest_leg_ranks[i]>highest_rank ? highest_rank=acc->highest_leg_ranks[i] : 0;

        if(acc->uplink!=NULL)
        {
            if(acc==acc->uplink->children->account)
            {
                acc->uplink->leg_volumes[0] += points;
                
                /* uplink highest leg-rank could change due to new account rank*/
                acc->uplink->highest_leg_ranks[0]<acc->rank ? (acc->uplink->highest_leg_ranks[0]=acc->rank) : 0;

                /* uplink highest leg-rank could change due to new highest account leg-rank*/
                acc->uplink->highest_leg_ranks[0]<highest_rank ? 
                    (acc->uplink->highest_leg_ranks[0]=highest_rank) : 0;
            }
            else if(acc==acc->uplink->children->next->account)
            {
                acc->uplink->leg_volumes[1] += points;
                
                /* uplink highest leg-rank could change due to new account rank*/
                acc->uplink->highest_leg_ranks[1]<acc->rank ? (acc->uplink->highest_leg_ranks[1]=acc->rank) : 0;

                /* uplink highest leg-rank could change due to new highest account leg-rank*/
                acc->uplink->highest_leg_ranks[1]<highest_rank ? 
                    (acc->uplink->highest_leg_ranks[1]=highest_rank) : 0;

            }
            else 
            {
                acc->uplink->leg_volumes[2] += points;
                
                /* uplink highest leg-rank could change due to new account rank*/
                acc->uplink->highest_leg_ranks[2]<acc->rank ? (acc->uplink->highest_leg_ranks[2]=acc->rank) : 0;

                /* uplink highest leg-rank could change due to new highest account leg-rank*/
                acc->uplink->highest_leg_ranks[2]<highest_rank ? 
                    (acc->uplink->highest_leg_ranks[2]=highest_rank) : 0;

            }
        }
        acc = acc->uplink;
    }
    
}

void award_commission(Account account, Amount points, String commission_type, String reason, FILE *fout)
{
    if(!points){ghfu_warn(8,fout); return;}
    if(account==NULL) {fprintf(fout,"could not award commissions for NULL account!\n"); return;}
    
    Commission new_commission = (Commission)malloc(sizeof(struct commission));
    if(new_commission==NULL) memerror(fout);

    new_commission->reason = NULL;
    new_commission->amount = 0;

    new_commission->next = NULL;
    new_commission->prev = NULL;

    Amount p_comm=0;
    char points_str[16];
    unsigned int buff_length;
    
    bool update_pv = false;
    
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
        
        update_pv = true;        
    }
    else if(!strcmp(commission_type, "DRA")) {p_comm = points;} /*director's recorgnition award*/    
    else if(!strcmp(commission_type, "HOB")) {p_comm = HOB*0.01*points;}    
    else if(!strcmp(commission_type, "LCB")) {p_comm = LCB*0.01*points;}    
    else if(!strcmp(commission_type, "EAB")) {p_comm = EAB*0.01*points;}    
    else if(!strcmp(commission_type, "TBB"))
    {
        /* team-building-bonus (whenever a new member joins)*/
        
        gfree(new_commission); /* you dont want memory leaks, trust me */
        
        if(account->uplink==NULL) return;
        int generation = 1;
        Account uplink = account->uplink->uplink;
        
        Commission last_commission;
        
        while((generation<TBB_MAX_GENERATIONS) && (uplink!=NULL))
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
                if(uplink->last_commission==NULL) memerror(fout);

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
                if(uplink->last_commission->reason==NULL) memerror(fout);
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
        if(new_commission->reason==NULL) memerror(fout); /* could do much better to rollback all changes 
                                                            but i dint coz memerror exits the program!*/
        join_strings(new_commission->reason,reason_strings);

        new_commission->amount = p_comm;
        account->available_balance += p_comm;
        account->total_returns += p_comm;

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

        if(update_pv) increment_pv(account,points, fout);

        CUMULATIVE_COMMISSIONS += p_comm;
        COMMISSIONS += p_comm;
    }
}

bool invest(ID account_id, const Amount amount, const String package, const ID package_id, const bool update_system_float, String fout_name)
{
    /*python/java/etc interface to invest_money*/

    Account account = get_account_by_id(account_id);

    FILE *fout = fopen(fout_name, "w");
    bool invested = invest_money(account,amount, package, package_id, update_system_float, fout);

    fclose(fout);
    
    return invested;
}

bool invest_money(Account account, Amount amount, String package, ID package_id, bool update_system_float, FILE *fout)
{
    if(account==NULL) {fprintf(fout,"could not create investment. Invalid account provided!\n"); return false;}

    amount -= OPERATIONS_FEE;
    if(amount<0){ printf("failed to invest for <%s>...", account->names); ghfu_warn(4,fout); return false; }
    if(!amount){ printf("failed to invest for <%s>...", account->names); ghfu_warn(5,fout); return false; }
    if(amount<MINIMUM_INVESTMENT){ printf("failed to invest for <%s>...", account->names); ghfu_warn(6,fout); return false; }
    if(amount>MAXIMUM_INVESTMENT){ printf("failed to invest for <%s>...", account->names); ghfu_warn(9,fout); return false; }

    Investment new_investment = (Investment)malloc(sizeof(struct investment));
    
    if(new_investment==NULL) memerror(fout);

    Amount points = amount*POINT_FACTOR;
    
    time(&(new_investment->date));

    /* we dont know that package will point to after we exit here so we create space for it on the heap 
       i witnessed this issue when calling libjermGHFU.so->invest from python. see that the commission
       reason in award_commission does the same
    */

    new_investment->amount = points;
    new_investment->package = NULL;
    new_investment->package_id = package_id;

    new_investment->returns = 0;
    new_investment->months_returned = 0;

    new_investment->next = NULL;
    new_investment->prev = NULL;

    unsigned int buff_length;
    String reason_strings[] = {package, "\0"};
    length_of_all_strings(reason_strings, &buff_length);

    new_investment->package = (String)malloc(sizeof(char)*(buff_length+1));
    if(new_investment->package==NULL) {gfree(new_investment); memerror(fout);}
    join_strings(new_investment->package,reason_strings);

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

    if(update_system_float)
    {   
        if (account->uplink!=NULL)
        {
            String c_reason_strings[] = {"FSB from ", account->names,"'s investment", "\0"};
            length_of_all_strings(c_reason_strings, &buff_length);
            char c_reason[buff_length+1];
            join_strings(c_reason, c_reason_strings);
            award_commission(account->uplink,points,"FSB",c_reason, fout);
        }
        SYSTEM_FLOAT += amount;
    }

    increment_pv(account, points, fout);

    return true; /* leave this out and you have memory leaks when invest money is called from regiter_member*/
}

ID register_new_member(ID uplink_id, String names, Amount amount, String fout_name)
{
    /* if return value==0, error occured, otherwise, new account ID is returned */

    /* python/java/etc interface to register_member */
    FILE *fout = fopen(fout_name,"a");

    ID new_id;

    if(uplink_id && get_account_by_id(uplink_id)==NULL){fprintf(fout, "uplink does not exist!"); new_id=0;}
    else
        new_id = account_id(register_member(get_account_by_id(uplink_id), names, amount, fout));
    
    fclose(fout);
    
    return new_id;
}

Account register_member(Account uplink, String names, Amount amount, FILE *fout)
{
    /* if this function returns NULL, DONT USE RESULT (it indicates an error, more like malloc)*/
    if(amount<(ACCOUNT_CREATION_FEE + ANNUAL_SUBSCRIPTION_FEE))
        { fprintf(fout, "failed to add <%s>...",names); ghfu_warn(1,fout); return NULL; }
    if(amount>ACCOUNT_CREATION_FEE + ANNUAL_SUBSCRIPTION_FEE+MAXIMUM_INVESTMENT+OPERATIONS_FEE)
        { fprintf(fout, "failed to add <%s>...",names); ghfu_warn(9,fout); return NULL; }

    Account new_account = (Account)malloc(sizeof(struct account));
    
    if(new_account==NULL) memerror(fout);

    new_account->id = CURRENT_ID+1;
    new_account->names = NULL;
    new_account->pv = 0;
    new_account->available_balance = 0;
    new_account->total_returns = 0;
    new_account->total_redeems = 0;
    
    new_account->uplink = uplink;
    new_account->children = NULL;
    new_account->last_child = NULL;
    new_account->commissions = NULL;
    new_account->last_commission = NULL;

    new_account->leg_volumes[0] = 0.0;
    new_account->leg_volumes[1] = 0.0;
    new_account->leg_volumes[2] = 0.0;

    new_account->TVC_levels[0] = 0;
    new_account->TVC_levels[1] = 0;

    new_account->investments = NULL;
    new_account->last_investment = NULL;

    new_account->rank = 0;

    new_account->highest_leg_ranks[0] = 0.0;
    new_account->highest_leg_ranks[1] = 0.0;
    new_account->highest_leg_ranks[2] = 0.0;

    unsigned int buff_length;
    String name_strings[] = {names, "\0"};
    length_of_all_strings(name_strings, &buff_length);

    new_account->names = (String)malloc(sizeof(char)*(buff_length+1));
    if(new_account->names==NULL) {gfree(new_account); memerror(fout);}
    join_strings(new_account->names,name_strings);


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
            gfree(new_account->names);
            gfree(new_account); /* you dont want memory leaks, trust me */
            memerror(fout);
        }
        
        ap->account = new_account;
        ap->next = NULL;

        if(uplink->children==NULL) { ap->prev = NULL; uplink->children = ap;}
        else { ap->prev = uplink->last_child; uplink->last_child->next = ap;}

        uplink->last_child = ap;
        
        if(points)
        {
            /* create reason for the commission and award the commission */
            String reason_strings[] = {"FSB from adding ",names,"\0"};
            unsigned int buff_length;
            length_of_all_strings(reason_strings, &buff_length);
            char buff[buff_length+1];
            join_strings(buff,reason_strings);

            award_commission(uplink, points, "FSB",buff, fout);
        }
    }
    else {gfree(ap1); /* you dont want memory leaks, trust me */ }

    if(points) award_commission(new_account, points,"TBB","", fout);

    /* add new node to linear structure of all nodes */
    AccountPointer ap = (AccountPointer)malloc(sizeof(struct account_pointer));
    if(ap==NULL)
    {
         /* you dont want memory leaks, trust me */
        gfree(new_account->names);
        if(uplink!=NULL) gfree(ap1);
        gfree(new_account);
        
        memerror(fout);
    }



    /* attempt to invest money (after declaring that new_member is a child of uplink; this is important
       because invest_money calls incrememt_pv which needs to call children of uplinks)*/
    if(points && !invest_money(new_account, amount, "Investment", 0, false, fout))
    {
        if(uplink!=NULL)
        {
            if(uplink->children==uplink->last_child)
                {uplink->children=NULL; uplink->last_child=uplink->children;}
            else
                {uplink->last_child = uplink->last_child->prev; uplink->last_child->next=NULL;}
            gfree(ap1);
        }
        gfree(ap);
        gfree(new_account->names);
        gfree(new_account);
        return NULL;
    }

    ap->account = new_account;
    ap->next = NULL;

    if(HEAD->next==NULL) { ap->prev = HEAD; HEAD->next = ap; }
    else { ap->prev = TAIL; TAIL->next = ap; }
    
    TAIL = ap;

    /* update system float */
    SYSTEM_FLOAT += _amount;

    ++CURRENT_ID; ++ACTIVE_ACCOUNTS;

    return new_account;
}

void buy_property(Account IB_account, Amount amount, bool member, String buyer_names, FILE *fout)
{
    /* if member(IV or registered investor), get IBC and those points otherwise just add this amount to 
       the system total float */
    Amount points = amount*POINT_FACTOR;
        
    if(IB_account==NULL){ghfu_warn(3,fout); return;}

    /* create reason for the commission */
    String reason_strings[] = {"IBC from ",buyer_names,"\0"};
    unsigned int buff_length;
    length_of_all_strings(reason_strings, &buff_length);
    char reason[buff_length+1];
    join_strings(reason,reason_strings);

    award_commission(IB_account, points, "IBC", reason,fout);

    if (IB_account->uplink!=NULL)
    {
        String c_reason_strings[] = {"FSB from ", IB_account->names,"'s package purchase", "\0"};
        length_of_all_strings(c_reason_strings, &buff_length);
        char c_reason[buff_length+1];
        join_strings(c_reason, c_reason_strings);
        award_commission(IB_account->uplink,points,"FSB",c_reason, fout);
    }

    SYSTEM_FLOAT += amount;
}

void purchase_property(ID IB_account_id, const Amount amount, const bool member, const String buyer_names, String fout_name)
{
    /* python/java/etc interface to buy_property*/
    Account IB_account = get_account_by_id(IB_account_id);

    FILE *fout = fopen(fout_name, "w");
    buy_property(IB_account,amount,member,buyer_names,fout);
    
    fclose(fout);
}

void auto_refill(Account account, float percentages[][2], FILE *fout)
{
    /* if account==NULL, calculate autp-refills for all investments in the system otherwise, 
       calculate auto-refill for only that account's investments 
    */

    /* this function is called once every month (on an agreed date) */

    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    time_t t; time(&t);
    struct tm *today = localtime(&t);

    if(today->tm_mday!=PAYMENT_DAY){ghfu_warn(12,fout); return;}

    Commission new_commission;
    Investment inv;
    unsigned int i, buff_length;
    Amount returns;

    char returns_str[16], points_str[32], active_percentage_str[16];

    if(account!=NULL)
    {
        if(account->investments!=NULL)
        {
           inv = account->investments;
           while(inv!=NULL)
           {
               if(inv->months_returned<12)
               {
                    i = 0;
                    for(; percentages[i][0]; ++i)
                    {
                        if(inv->amount >= percentages[i][0]) break;
                    }
                    
                    returns = (inv->amount)*(percentages[i][1])*.01;

                    if(!percentages[i][0]) ghfu_warn(13,fout);
                    else
                    {
                        new_commission = (Commission)malloc(sizeof(struct commission));
                        if(new_commission==NULL) memerror(fout);

                        new_commission->reason = NULL;
                        new_commission->amount = returns;

                        new_commission->next = NULL;
                        new_commission->prev = NULL;

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

                        sprintf(returns_str," ($%.2f)",returns);
                        sprintf(points_str," worth %.2f points",inv->amount);
                        sprintf(active_percentage_str," %.2f%%",percentages[i][1]);
                        String reason_strings[] = {"Investment return for package <", 
                            inv->package, "> ",points_str ,returns_str, 
                            " given at", active_percentage_str, "\0"};
                        length_of_all_strings(reason_strings, &buff_length);

                        /* create a new array to hold this commission reason (you cant just assign a local char[] to
                           new_commission->reason...basically U DONT WANNA ASSIGN TO A LOCAL POINTER BCOZ ONCE D FUCTION
                           EXITS, THE POINTER WILL BE ANONYMOUS!)*/
                        new_commission->reason = (String)malloc(sizeof(char)*(buff_length+1));
                        if(new_commission->reason==NULL) {gfree(new_commission); memerror(fout);}
                        join_strings(new_commission->reason,reason_strings);

                        ++(inv->months_returned);
                        account->available_balance += returns;
                        account->total_returns += returns;
                        
                        CUMULATIVE_COMMISSIONS += returns;
                        COMMISSIONS += returns;
                        
                    }
                   
               }
               inv = inv->next;
           } 
        }
        return;
    }

    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = acc_p==NULL ? acc_p : acc_p->next;

    while(acc_p!=NULL)
    {
        acc = acc_p->account;

        if(acc->investments!=NULL)
        {
           inv = acc->investments;
           while(inv!=NULL)
           {
               if(inv->months_returned<12)
               {
                    i = 0;
                    for(; percentages[i][0]; ++i)
                    {
                        if(inv->amount >= percentages[i][0]) break;
                    }
                    
                    returns = (inv->amount)*(percentages[i][1])*.01;

                    if(!percentages[i][0]) ghfu_warn(13,fout);
                    else
                    {
                        new_commission = (Commission)malloc(sizeof(struct commission));
                        if(new_commission==NULL) memerror(fout);

                        new_commission->reason = NULL;
                        new_commission->amount = returns;

                        new_commission->next = NULL;
                        new_commission->prev = NULL;

                        if(acc->commissions==NULL)
                        {
                            acc->commissions = new_commission;
                            acc->last_commission = acc->commissions;
                        }
                        else
                        {
                            new_commission->prev = acc->last_commission;
                            acc->last_commission->next = new_commission;
                            acc->last_commission = new_commission;
                        }

                        sprintf(returns_str," ($%.2f)",returns);
                        sprintf(points_str," worth %.2f points",inv->amount);
                        sprintf(active_percentage_str," %.2f%%",percentages[i][1]);
                        String reason_strings[] = {"Investment return for package <", 
                            inv->package, "> ",points_str ,returns_str, 
                            " given at", active_percentage_str, "\0"};
                        length_of_all_strings(reason_strings, &buff_length);

                        /* create a new array to hold this commission reason (you cant just assign a local char[] to
                           new_commission->reason...basically U DONT WANNA ASSIGN TO A LOCAL POINTER BCOZ ONCE D FUCTION
                           EXITS, THE POINTER WILL BE ANONYMOUS!)*/
                        new_commission->reason = (String)malloc(sizeof(char)*(buff_length+1));
                        if(new_commission->reason==NULL) {gfree(new_commission); memerror(fout);}
                        join_strings(new_commission->reason,reason_strings);

                        ++(inv->months_returned);
                        acc->available_balance += returns;
                        acc->total_returns += returns;

                        CUMULATIVE_COMMISSIONS += returns;
                        COMMISSIONS += returns;
                        
                    }
                   
               }
               inv = inv->next;
           } 
        }


        acc_p = acc_p->next;
    }
}

bool raise_rank(Account account, FILE *fout)
{
    /* if true; award onetime Director's recorgintion award...
       however, the award is in such a way that its descrete ie if one leaps
       from rank r to (r+4), they get the award for (r+4) not all the awards from
       (r+1) through (r+4)!
    */
    bool rank_raised = false;
    if(account==NULL) return rank_raised;
    if(account->rank==11) return rank_raised; /* already at maximum rank */

    unsigned int rank = account->rank;

    while((RANK_DETAILS[rank][3]!=1) && ++rank)
    {
        if(!(
                (account->highest_leg_ranks[0]>=RANK_DETAILS[rank][0]) &&
                (account->highest_leg_ranks[1]>=RANK_DETAILS[rank][1]) &&
                (account->highest_leg_ranks[2]>=RANK_DETAILS[rank][2]) &&

                (account->pv>=RANK_DETAILS[rank][3]) &&
                ((account->leg_volumes[0]+account->leg_volumes[1] +
                    account->leg_volumes[2])>=RANK_DETAILS[rank][4]) &&

                /* lastly, check for the lesser-leg volume */
                ((account->leg_volumes[0]<account->leg_volumes[1] ? 
                    (account->leg_volumes[0]<account->leg_volumes[2] ? 
                        account->leg_volumes[0] : account->leg_volumes[2]
                    ) 
                    : 
                    (account->leg_volumes[1]<account->leg_volumes[2] ? 
                        account->leg_volumes[1] : account->leg_volumes[2]
                    )
                ) >= RANK_DETAILS[rank][5])
        )) {--rank; break;}
    }

    rank = rank==12 ? --rank : rank;

    if(rank==account->rank) return rank_raised;

    rank_raised = true;

    unsigned int buff_length=0;
    String reason_strings[] = {"DRA, new rank is <", RANKS[rank], ">", "\0"};
    length_of_all_strings(reason_strings, &buff_length);
    char buff[buff_length+1];
    join_strings(buff,reason_strings);
    
    if(RANK_DETAILS[rank][6]) award_commission(account, RANK_DETAILS[rank][6], "DRA", buff, fout);
    
    account->rank = rank;
    
    return rank_raised;
}

void calculate_tvc(Account account, FILE *fout)
{
    /* this function should be called only once a month on the agreed day */

    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    time_t t; time(&t);
    struct tm *today = localtime(&t);

    if(today->tm_mday!=PAYMENT_DAY){ghfu_warn(15,fout); return;}

    Amount lower_leg_volume, actual_lower_leg_volume, returns;
    unsigned int i, j, buff_length; /* i is used for getting the lower_leg_volume, while j is used to
                              get the TVC % comission based on the account PV
                           */

    Commission new_commission;

    char returns_str[16], llv_str[32], allv_str[32];

    if(account!=NULL)
    {
        if(
            /* since logic operators are processed left to right, this code won't 
               break when account->children->next==NULL!
            */
           (account->children!=NULL) && 
           (account->children->next!=NULL) && 
           /* in addition to having atleast 2 direct-children, each child must have 
              atleast 1 direct child of their own */
           (((Account)(account->children->account))->children!=NULL) &&
           (((Account)(account->children->next->account))->children!=NULL)
           )
        {
            lower_leg_volume = account->leg_volumes[0];
            i = 0;
            for(; i<2/*only he first 2 legs considered*/; ++i)
                lower_leg_volume = (account->leg_volumes[i])<lower_leg_volume ? 
                    account->leg_volumes[i] : lower_leg_volume ;

            actual_lower_leg_volume = lower_leg_volume - account->TVC_levels[0];
            account->TVC_levels[0] = lower_leg_volume;
            
            j = 0;
            for(;TVC[j][1];++j)
            {
                if(account->pv < TVC[i+1][1]) break;
            }
            
            j = TVC[j][1] ? j : --j;
            
            returns = TVC[j][1]*.01*actual_lower_leg_volume;
            /* returns should not be more than PV made in the first month!!!!
               i rather changed this condition to "thou shalt not get TVC>375 points"
            */
            returns = returns>(MAXIMUM_INVESTMENT*POINT_FACTOR) ? MAXIMUM_INVESTMENT*POINT_FACTOR : returns;
            
            if(actual_lower_leg_volume) /* you dont wanna waste resources on 0-value commissions */
            {
                        new_commission = (Commission)malloc(sizeof(struct commission));
                        if(new_commission==NULL) memerror(fout);

                        new_commission->reason = NULL;
                        new_commission->amount = returns;

                        new_commission->next = NULL;
                        new_commission->prev = NULL;

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

                        sprintf(returns_str," ($%.2f)",returns);
                        sprintf(llv_str," %.2f points",lower_leg_volume);
                        sprintf(allv_str," %.2f points",actual_lower_leg_volume);
                        String reason_strings[] = {"TVC of", returns_str, ". LLV =",
                            llv_str, ", ALLV =", allv_str,"\0"};
                        length_of_all_strings(reason_strings, &buff_length);

                        /* create a new array to hold this commission reason (you cant just assign a local char[] to
                           new_commission->reason...basically U DONT WANNA ASSIGN TO A LOCAL POINTER BCOZ ONCE D FUCTION
                           EXITS, THE POINTER WILL BE ANONYMOUS!)*/
                        new_commission->reason = (String)malloc(sizeof(char)*(buff_length+1));
                        if(new_commission->reason==NULL) {gfree(new_commission); memerror(fout);}
                        join_strings(new_commission->reason,reason_strings);

                        account->available_balance += returns;
                        account->total_returns += returns;
                        
                        CUMULATIVE_COMMISSIONS += returns;
                        COMMISSIONS += returns;
            
            }
        }
        
        /* else{fprintf(fout,"not TVC for <%s>...",account->names); ghfu_warn(14,fout);} */
        
        return;
    }

    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = acc_p==NULL ? acc_p : acc_p->next;

    while(acc_p!=NULL)
    {
        acc = acc_p->account;

        if(
            /* since logic operators are processed left to right, this code won't 
               break when account->children->next==NULL or when account->children->next->next==NULL!
            */
           (acc->children!=NULL) && 
           (acc->children->next!=NULL) && 
           /* in addition to having atleast 2 direct-children, each child must have 
              atleast 1 direct child of their own */
           (((Account)(acc->children->account))->children!=NULL) &&
           (((Account)(acc->children->next->account))->children!=NULL)
           )
        {
            lower_leg_volume = acc->leg_volumes[0];
            i = 0;
            for(; i<2/*only he first 2 legs considered*/; ++i)
                lower_leg_volume = (acc->leg_volumes[i])<lower_leg_volume ? 
                    acc->leg_volumes[i] : lower_leg_volume ;

            actual_lower_leg_volume = lower_leg_volume - acc->TVC_levels[0];
            acc->TVC_levels[0] = lower_leg_volume;
            
            j = 0;
            for(;TVC[j][1];++j)
            {
                if(acc->pv < TVC[i+1][1]) break;
            }
            
            j = TVC[j][1] ? j : --j;
            
            returns = TVC[j][1]*.01*actual_lower_leg_volume;
            /* returns should not be more than PV made in the first month!!!!
               i rather changed this condition to "thou shalt not get TVC>375 points"
            */
            returns = returns>(MAXIMUM_INVESTMENT*POINT_FACTOR) ? MAXIMUM_INVESTMENT*POINT_FACTOR : returns;
            
            if(actual_lower_leg_volume) /* you dont wanna waste resources on 0-value commissions */
            {
                        new_commission = (Commission)malloc(sizeof(struct commission));
                        if(new_commission==NULL) memerror(fout);

                        new_commission->reason = NULL;
                        new_commission->amount = returns;

                        new_commission->next = NULL;
                        new_commission->prev = NULL;

                        if(acc->commissions==NULL)
                        {
                            acc->commissions = new_commission;
                            acc->last_commission = acc->commissions;
                        }
                        else
                        {
                            new_commission->prev = acc->last_commission;
                            acc->last_commission->next = new_commission;
                            acc->last_commission = new_commission;
                        }

                        sprintf(returns_str," ($%.2f)",returns);
                        sprintf(llv_str," %.2f points",lower_leg_volume);
                        sprintf(allv_str," %.2f points",actual_lower_leg_volume);
                        String reason_strings[] = {"TVC of", returns_str, ". LLV =",
                            llv_str, ", ALLV =", allv_str,"\0"};
                        length_of_all_strings(reason_strings, &buff_length);

                        /* create a new array to hold this commission reason (you cant just assign a local char[] to
                           new_commission->reason...basically U DONT WANNA ASSIGN TO A LOCAL POINTER BCOZ ONCE D FUCTION
                           EXITS, THE POINTER WILL BE ANONYMOUS!)*/
                        new_commission->reason = (String)malloc(sizeof(char)*(buff_length+1));
                        if(new_commission->reason==NULL) {gfree(new_commission); memerror(fout);}
                        join_strings(new_commission->reason,reason_strings);

                        acc->available_balance += returns;
                        acc->total_returns += returns;
                        
                        CUMULATIVE_COMMISSIONS += returns;
                        COMMISSIONS += returns;
            
            }
        }

        acc_p = acc_p->next;
    }
}

void award_rank_monthly_bonuses(Account account, FILE *fout)
{
    Amount lower_leg_volume, actual_lower_leg_volume;
    unsigned int i;

    if(account!=NULL)
    {
        if(account->rank>=6) /* rank-bonuses for rank>=Senior Director*/
        {
            lower_leg_volume = account->leg_volumes[0];
            i = 0;
            for(; i<3; ++i)
                lower_leg_volume = (account->leg_volumes[i])<lower_leg_volume ? 
                    account->leg_volumes[i] : lower_leg_volume ;

            actual_lower_leg_volume = lower_leg_volume - account->TVC_levels[1];
            account->TVC_levels[1] = lower_leg_volume;
            
            if(actual_lower_leg_volume)
            {
                award_commission(account, actual_lower_leg_volume, "HOB", "HOB",fout);
                if(account->rank>=8)
                {
                    award_commission(account, actual_lower_leg_volume, "LCB", "LCB",fout);
                    award_commission(account, actual_lower_leg_volume, "EAB", "EA bonus",fout);
                }
            }
        }

        return;
    }
    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = acc_p==NULL ? acc_p : acc_p->next;

    while(acc_p!=NULL)
    {
        acc = acc_p->account;

        if(acc->rank>=6) /* rank-bonuses for rank>=Senior Director*/
        {
            lower_leg_volume = acc->leg_volumes[0];
            i = 0;
            for(; i<3; ++i)
                lower_leg_volume = (acc->leg_volumes[i])<lower_leg_volume ? 
                    acc->leg_volumes[i] : lower_leg_volume ;

            actual_lower_leg_volume = lower_leg_volume - acc->TVC_levels[1];
            acc->TVC_levels[1] = lower_leg_volume;
            
            if(actual_lower_leg_volume)
            {
                award_commission(acc, actual_lower_leg_volume, "HOB", "HOB",fout);
                if(acc->rank>=8)
                {
                    award_commission(acc, actual_lower_leg_volume, "LCB", "LCB",fout);
                    award_commission(acc, actual_lower_leg_volume, "EAB", "EA bonus",fout);
                }
            }
        }

        acc_p = acc_p->next;
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

void dump_commissions(const Account account, FILE *fout)
{
    /* dump commissions to json file */
    Commission c;
    
    if (account!=NULL)
    {
        c = account->commissions;
        fprintf(fout, ",\"commissions\": [");

        bool started=false;
        while(c!=NULL)
        {
            if(started) fprintf(fout,",");
            started = started ? started : true;
            fprintf(fout,"[%.2f,\"%s\"]",c->amount, c->reason);
            c=c->next;
            
        }
        
        fprintf(fout, "]");
    
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

void dump_leg_volumes(const Account account, FILE *fout)
{
    /* dump leg volumes to json file */
    if(account!=NULL)
    {
        fprintf(fout, ",\"leg_volumes\":[%.2f,%.2f,%.2f]",
            account->leg_volumes[0], account->leg_volumes[1], account->leg_volumes[2]);
    }
}

void show_investments(const Account account)
{
    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    Investment inv;
    struct tm *lt;
    if(account!=NULL)
    {
        printf("\n  %s's INVESTMENTS\n",account->names);

        inv = account->investments;

        if(inv==NULL) printf("    None\n");
        
        while(inv!=NULL)
        {
            lt = localtime(&(inv->date));
            printf("    Date (DD/MM/YYY): %d:%d, %d/%d/%d\n",
                lt->tm_hour,lt->tm_min,lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900));
            printf("    Weight: %.2f points\n",inv->amount);
            printf("    Package: %s\n",inv->package);
            printf("    Months returned: %d\n",inv->months_returned);
            
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
            lt = localtime(&(inv->date));
            printf("    Date (DD/MM/YYY): %d:%d, %d/%d/%d\n",
                lt->tm_hour,lt->tm_min,lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900));
            printf("    Weight: %.2f points\n",inv->amount);
            printf("    Package: %s\n",inv->package);
            printf("    Months returned: %d\n",inv->months_returned);
            
            inv = inv->next;
            
            if(inv!=NULL) printf("    --------\n");
        }

        acc_p = acc_p->next;
    }
}

void dump_investments(const Account account, FILE *fout)
{
    /* dump investments to json file */
    Investment inv;
    struct tm *lt;
    if(account!=NULL)
    {
        inv = account->investments;

        fprintf(fout, ",\"investments\":[");
        
        bool started = false;
        while(inv!=NULL)
        {
            if(started) fprintf(fout, ",");
            started = started ? started : true;

            lt = localtime(&(inv->date));

            fprintf(fout, "[\"%d:%d, %d/%d/%d\",%.2f,\"%s\",%ld,%d, %.2f]",
                lt->tm_hour,lt->tm_min,lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900),
                inv->amount,inv->package,inv->package_id,inv->months_returned, inv->returns);
            inv = inv->next;            
        }

        fprintf(fout, "]");
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

void dump_direct_children(const Account account, FILE *fout)
{
    /* dump account direct children to json file */
    AccountPointer acc_p;
    Account acc;

    if(account!=NULL)
    {
        acc_p = account->children;
        
        fprintf(fout, ",\"direct_children\":[");
        

        bool started = false;
        while(acc_p!=NULL)
        {
            if(started) fprintf(fout, ",");
            started = started ? started : true;
            acc = acc_p->account;
            fprintf(fout,"\"%s\"",acc->names); 

            acc_p=acc_p->next; 
        }

        fprintf(fout, "]");
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
        printf("  ID: %ld\n",account->id);
        printf("  Uplink: %s\n", account->uplink==NULL ? "ROOT" : account->uplink->names);
        printf("  PV = %.2lf points\n",account->pv);
        printf("  Total Returns = $%.2lf\n",account->total_returns);
        printf("  Available Balance = $%.2lf\n",account->available_balance);
        printf("  Total Redeemed = $%.2lf\n",account->total_redeems);
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
        printf("  ID: %ld\n",acc->id);
        printf("  Uplink: %s\n", acc->uplink==NULL ? "ROOT" : acc->uplink->names);
        printf("  PV = %.2lf points\n",acc->pv);
        printf("  Total Returns = $%.2lf\n",acc->total_returns);
        printf("  Available Balance = $%.2lf\n",acc->available_balance);
        printf("  Total Redeemed = $%.2lf\n",acc->total_redeems);
        printf("  Leg Volumes = (%.2lf, %.2lf, %.2lf)\n",
            acc->leg_volumes[0],acc->leg_volumes[1],acc->leg_volumes[2]);
        
        show_investments(acc);
        show_commissions(acc);
        show_direct_children(acc);

        acc_p = acc_p->next;
    }
    
}

bool dump_structure_details(ID account_id, String fname)
{
    /* this function creates a json file describing the account in all its detail. the json is then sent
       to the web API requesting for the account information
       fname is a full path to the output json file
    */

    Account account = get_account_by_id(account_id);

    if(account==NULL) return false;

    FILE *fout = fopen(fname, "w");
    if(!fout) return false;

    fprintf(fout, "{");

    /* personal info */
    fprintf(fout, "\"names\":\"%s\",\"id\":%ld,\"uplink\":\"%s\","
        "\"pv\":%.2f,\"total_returns\":%.2f,\"available_balance\":%.2f,\"total_redeems\":%.2f,"
        "\"rank\":\"%s\",\"highest-leg-ranks\":[\"%s\",\"%s\",\"%s\"]",
        account->names, account->id, (account->uplink==NULL ? "ROOT" : account->uplink->names),
        account->pv,account->total_returns,account->available_balance,account->total_redeems,
        RANKS[account->rank], RANKS[account->highest_leg_ranks[0]], RANKS[account->highest_leg_ranks[1]],
        RANKS[account->highest_leg_ranks[2]]);

    /* the other info */
    dump_commissions(account, fout);
    dump_leg_volumes(account,fout);
    dump_investments(account, fout);
    dump_direct_children(account, fout);

    fprintf(fout, "}");

    fclose(fout);


    /* now encrypt generated db_file(is it necessary? i doubt bcoz python/any other adaptor will delete
       the json file as soon as it reads from it) */

    return true;
}

bool redeem_points(Account account, Amount amount, FILE *fout)
{
    /* if this function returns true but the money actually is'nt redeemed for some reason (loss in
       network for example or when the mobile money API is down), please REVERSE this operation or the 
       member will actually lose their returns permanently! 
    */
    if(account==NULL){ghfu_warn(11,fout); return false;}
    if(amount>(account->available_balance)){ghfu_warn(10,fout); return false;}

    account->available_balance -= amount;
    account->total_redeems += amount;

    return true;
}

bool redeem_account_points(ID account_id, Amount amount, String fout_name)
{
    /* python/java/etc interface to redeem_points */

    Account account = get_account_by_id(account_id);
    FILE *fout = fopen(fout_name, "w");
    bool redeemed = redeem_points(account, amount, fout);

    fclose(fout);

    return redeemed;
}

void length_of_all_strings(String strings[], unsigned int *length)
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

Account get_account_by_id(const ID id)
{
    /* this function returns NULL if any of the following is true;
        1) id>CURRENT_ID
        2) HEAD==NULL (no member in system)
        3) HEAD->next==NULL (no member in system)
        4) the requested account was deleted and is nolonger in the system
    
    */
    if((id>CURRENT_ID) || (HEAD==NULL) || (HEAD->next==NULL)) return NULL;

    /* search from HEAD or from TAIL depending on if the id is closer to the last member id or not
       the problem with this however is that since the ID-counter never decrements(even when amember
       is deleted from the system), the id may be closer to the HEAD than the TAIL when a sufficiently
       large group of members have been deleted but the calculation will assume the id is still closer
       to the TAIL as only the TAIL id is considered!
    */
    AccountPointer acc_p = (TAIL==HEAD) ? HEAD->next :
        (id > (((Account)(TAIL->account))->id)/2 ? TAIL : HEAD->next);
    
    
    bool ascending = acc_p==HEAD->next ? true : false;
    
    while(acc_p!=NULL)
    {
        if(((Account)(acc_p->account))->id==id) break;

        /* break flow the moment its detected that the account was deleted. this is made easy by 
           the fact that id's are incremental */
        if(ascending && (((Account)(acc_p->account))->id > id)) return NULL;
        if((!ascending) && (((Account)(acc_p->account))->id < id)) return NULL;

        acc_p = ascending ? acc_p->next: acc_p->prev;
    }
    
    return acc_p==NULL ? NULL : (Account)(acc_p->account); /*if return is NULL, that account was deleted*/
}

ID account_id(Account account)
{
    if(account==NULL) return 0;
    return account->id;
}

void monthly_operations(float auto_refill_percentages[][2], FILE *fout)
{

    /* perform all monthly operations */
    auto_refill(NULL, auto_refill_percentages,fout);
    calculate_tvc(NULL,fout);
    award_rank_monthly_bonuses(NULL, fout);
}

void perform_monthly_operations(float auto_refill_percentages[][2], String fout_name)
{
    /* python/java/etc interface to monthly_operations */
    FILE *fout = fopen(fout_name, "w");
    monthly_operations(auto_refill_percentages, fout);
    
    fclose(fout);
}

void ghfu_warn(unsigned int ghfu_errno, FILE *fout)
{
    if(fout==stdin)
        fprintf(fout, "\033[33m%s\033[0m\n",ERRORS[ghfu_errno]);
    fprintf(fout, "%s\n",ERRORS[ghfu_errno]);    
}

void gfree(void *p)
{
    /* free mallc'd memory and set pointer to NULL */
    free(p);
    p = NULL;
}

/* data constants modifiers*/
bool set_constant(String constant, Amount value)
{        
    if (value<=0) return false; 
    
    bool status = true;
    
    if(!strcmp(constant, "payment-day")) PAYMENT_DAY = (int)value;
    else if(!strcmp(constant, "point-factor")) POINT_FACTOR = value;
    else if(!strcmp(constant, "account-creation-fee")) ACCOUNT_CREATION_FEE = value;
    else if(!strcmp(constant, "annual-subscription-fee")) ANNUAL_SUBSCRIPTION_FEE = value;
    else if(!strcmp(constant, "operations-fee")) OPERATIONS_FEE = value;
    else if(!strcmp(constant, "minimum-investment")) MINIMUM_INVESTMENT = value;
    else if(!strcmp(constant, "maximum-investment")) MAXIMUM_INVESTMENT = value;

    else status = false;

    /* dump changes to file so that init sets them the next time the system is rebooted */

    return status;
}

bool dump_constants(String jermCrypt_path, String save_dir)
{
    bool status = false;

    unsigned int buff_length;
    String crypt_lib_paths[] = {jermCrypt_path,"/",JERM_CRYPT_LIB,"\0"};
    String data_file_paths[] = {save_dir, "/",DATA_FILE,"\0"};
    
    length_of_all_strings(crypt_lib_paths, &buff_length);
    char crypt_lib_path[buff_length+1];
    join_strings(crypt_lib_path, crypt_lib_paths);

    length_of_all_strings(data_file_paths, &buff_length);
    char data_file_path[buff_length+1];
    join_strings(data_file_path, data_file_paths);

    FILE *fout = fopen(data_file_path,"wb");

    if(!fout) return status;

    /* load function from jermCrypt library eg; */
    void *libjermCrypt = dlopen(crypt_lib_path, RTLD_GLOBAL|RTLD_LAZY);

    if(!libjermCrypt) {fclose(fout); return status;}

    /* encrypt_file prototype in libjermCrypt.so */
    // char *encrypt_file(char *fin, char *pswd, char *fout, int overwrite, int verbose)
    
    char *(*encrypt_file)(char*,char*,char*,int,int) = dlsym(libjermCrypt, "encrypt_file");

    if(encrypt_file==NULL) return status;

    fprintf(fout, "%d\x1%f\x1%f\x1%f\x1%f\x1%f\x1%f\x1%f\x1%f\x1%f\x1%ld\x1%ld\x1", 
        PAYMENT_DAY,POINT_FACTOR,ACCOUNT_CREATION_FEE,ANNUAL_SUBSCRIPTION_FEE,OPERATIONS_FEE,
        MINIMUM_INVESTMENT,MAXIMUM_INVESTMENT,SYSTEM_FLOAT,CUMULATIVE_COMMISSIONS,COMMISSIONS,
        ACTIVE_ACCOUNTS,CURRENT_ID
        );

    /* dump current auto-refill percentages */

    int num_of_arps=0;
    for(; MONTHLY_AUTO_REFILL_PERCENTAGES[num_of_arps][0]; ++num_of_arps);
    fprintf(fout, "%d\x1", num_of_arps);
    for(int i=0; i<(num_of_arps-1); ++i) /*dont dump the last {0,0} coz we are certain its always there! */
        fprintf(fout, "%.2f\x1%.2f\x1", 
            MONTHLY_AUTO_REFILL_PERCENTAGES[i][0],MONTHLY_AUTO_REFILL_PERCENTAGES[i][1]);

    fclose(fout);
    
    //encrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true,false);
            
    status = true;

    dlclose(libjermCrypt);

    return status;
}

bool load_constants(String jermCrypt_path, String save_dir)
{
    bool status = false;

    unsigned int buff_length;
    String crypt_lib_paths[] = {jermCrypt_path,"/",JERM_CRYPT_LIB,"\0"};
    String data_file_paths[] = {save_dir, "/",DATA_FILE,"\0"};
    
    length_of_all_strings(crypt_lib_paths, &buff_length);
    char crypt_lib_path[buff_length+1];
    join_strings(crypt_lib_path, crypt_lib_paths);

    length_of_all_strings(data_file_paths, &buff_length);
    char data_file_path[buff_length+1];
    join_strings(data_file_path, data_file_paths);

    FILE *fin = fopen(data_file_path,"rb");

    if(!fin) return status;

    /* load function from jermCrypt library eg; */
    void *libjermCrypt = dlopen(crypt_lib_path, RTLD_GLOBAL|RTLD_LAZY);

    if(!libjermCrypt) {fclose(fin); return status;}

    /* encrypt_file & decrypt_file prototypes in libjermCrypt.so */
    // char *encrypt_file(char *fin, char *pswd, char *fout, int overwrite, int verbose);
    // char *decrypt_file(char *fin, char *pswd, char *fout, int overwrite, int verbose);

    char *(*encrypt_file)(char*,char*,char*,int,int) = dlsym(libjermCrypt, "encrypt_file");

    char *(*decrypt_file)(char*,char*,char*,int,int) = dlsym(libjermCrypt, "decrypt_file");

    if(encrypt_file==NULL || decrypt_file==NULL) return status;

    fclose(fin);

    //decrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true,false);

    fin = fopen(data_file_path,"rb");

    fscanf(fin, "%d\x1%f\x1%f\x1%f\x1%f\x1%f\x1%f\x1%f\x1%f\x1%f\x1%ld\x1%ld\x1", 
        &PAYMENT_DAY,&POINT_FACTOR,&ACCOUNT_CREATION_FEE,&ANNUAL_SUBSCRIPTION_FEE,&OPERATIONS_FEE,
        &MINIMUM_INVESTMENT,&MAXIMUM_INVESTMENT,&SYSTEM_FLOAT,&CUMULATIVE_COMMISSIONS,&COMMISSIONS,
        &ACTIVE_ACCOUNTS,&CURRENT_ID
        );

    fclose(fin);
    
    //encrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true,false);
            
    status = true;
    dlclose(libjermCrypt);

    return status;
}

bool save_structure(String jermCrypt_path, String save_dir)
{
    if(HEAD==NULL) return true; /* thereis nothing to save, so no error !*/
    
    bool status = false;

    unsigned int buff_length;
    String crypt_lib_paths[] = {jermCrypt_path,"/",JERM_CRYPT_LIB,"\0"};
    String data_file_paths[] = {save_dir, "/",STRUCTURE_FILE,"\0"};
    
    length_of_all_strings(crypt_lib_paths, &buff_length);
    char crypt_lib_path[buff_length+1];
    join_strings(crypt_lib_path, crypt_lib_paths);

    length_of_all_strings(data_file_paths, &buff_length);
    char data_file_path[buff_length+1];
    join_strings(data_file_path, data_file_paths);

    FILE *fout = fopen(data_file_path,"wb");

    if(!fout) return status;

    /* load function from jermCrypt library eg; */
    void *libjermCrypt = dlopen(crypt_lib_path, RTLD_GLOBAL|RTLD_LAZY);

    if(!libjermCrypt) {fclose(fout); return status;}

    /* encrypt_file prototype in libjermCrypt.so */
    // char *encrypt_file(char *fin, char *pswd, char *fout, int overwrite, int verbose)
    
    char *(*encrypt_file)(char*,char*,char*,int,int) = dlsym(libjermCrypt, "encrypt_file");

    if(encrypt_file==NULL) {dlclose(libjermCrypt); return status;}

    /* dump entire structure to the file now... */

    AccountPointer acc_p = HEAD->next, child;
    Account acc;

    Commission commission;
    Investment investment;

    unsigned long number_of_items;

    while(acc_p!=NULL)
    {
        /*       dumping logic:
            \x1: separation xter btn all data
            
            1) dump all numerical values(id,pv,etc) first
            2) dump the length of the account names followed by the names
            3) dump the number of direct-children then the direct children ids
            4) dump the number of commissions and then for each commission, dump the amount then
               the length of the reason
            5) dump the number of investments and then for each investment, dump the date then
               the amount, returns, months returned, package id, the length of the package name 
               and the package name itself
               
               NB: since the last attr to be represented is a string whse length is known(if there
                   was an investment), there is no need to separate different accounts with \x1
            
            an example is;
            account details:
                id: 5
                names: "glayn bukman"
                pv: 56.23
                available_balance: 300.89
                total_returns: 23.56
                total_redeems: 0.00
                uplink_id: 3 // NULL(ROOT)->id = 0
                leg_volumes: [56.23,124.98,489.26]
                TVC_levels: [869,248.79]
                rank: 0
                highest_leg_ranks: [1,1,0]
                children_ids:  [3,4,9]
                commissions: [(23.56, "reason 1"), (56.18,"wow this is cool!")]
                
                //investment data format: date,amount,pkg-name,pkg-id,months-returned,total-returns
                investments: [(13625468,200.03,"pkg 1", 23,3,163.89)]
             
                the output would be(neglect the newlines);
                
                5\x156.23\x1300.89\x123.56\x10.00\x13\x156.23\x1124.98\x1489.26\x1869\x1248.79\x10
                \x11\x11\x10\x1
                12\x1glayn bukman
                3\x14\x19\x1
                2\x123.56\x18\x1reason 156.18\x117\x1wow this is cool!
                1\x113625468\x1200.03\x1163.89\x13\x1
                23\x15\x1pkg 1
             
            NB: after dumping strings, dont use the separation xter(bcoz the length of the string
                dumped just b4 it tells us exactly where to stop scanning for the string!). however,
                its necessay to put a separation xter between the length of the string and the string 
                because in a very wiered scenario, the first xters of the string may contaain valid
                numbers eg "23; that was the number of commisions awarded!"
            
            if this logic is not clear, you dont wanna read thru the load_structure function!
            Also, the exact implementation order may change but the logic still holds
        */
        acc = acc_p->account;
        
        fprintf(fout, 
                /*section 1 (first-hand numeric attributes of the member + names)*/
            "%ld\x1%.2f\x1%.2f\x1%.2f\x1%.2f\x1%ld\x1%.2f\x1%.2f\x1%.2f\x1%.2f\x1%.2f\x1%d\x1"
            "%d\x1%d\x1%d\x1%ld\x1%s",
            acc->id,acc->pv,acc->available_balance,acc->total_returns,acc->total_redeems,
            (acc->uplink==NULL ? 0 : acc->uplink->id), acc->leg_volumes[0], acc->leg_volumes[1],
            acc->leg_volumes[2],acc->TVC_levels[0],acc->TVC_levels[1], acc->rank,
            acc->highest_leg_ranks[0],acc->highest_leg_ranks[1],acc->highest_leg_ranks[2],
            strlen(acc->names),acc->names
        );

        /* start next section (children)*/

        child = acc->children;
        number_of_items = 0;
        if(child==NULL) 
            fprintf(fout,"%ld\x1",number_of_items); 
        else
        {
            while(child!=NULL) {++number_of_items; child=child->next;}
            fprintf(fout,"%ld\x1",number_of_items); 

            child = acc->children;

            while(child!=NULL)
            {
                fprintf(fout, "%ld\x1",((Account)(child->account))->id);
                child = child->next;
            }
        }

        /* start next section (commissions)*/

        commission = acc->commissions;
        number_of_items = 0;
        if(commission==NULL)
            fprintf(fout, "%ld\x1",number_of_items);
        else
        {
            while(commission!=NULL){++number_of_items; commission=commission->next;}
            fprintf(fout, "%ld\x1",number_of_items);

            commission = acc->commissions;

            while(commission!=NULL)
            {
                fprintf(fout, "%.2f\x1%ld\x1%s",commission->amount, strlen(commission->reason), 
                    commission->reason);
                commission = commission->next;
            }
        }

        /* start next section (investments)*/
        number_of_items = 0;
        investment = acc->investments;

        if(investment==NULL)
            fprintf(fout, "%ld\x1",number_of_items);
        else
        {
            while(investment!=NULL){++number_of_items; investment=investment->next;}
            fprintf(fout, "%ld\x1",number_of_items);

            investment = acc->investments;
            while(investment!=NULL)
            {
                fprintf(fout, "%ld\x1%.2f\x1%ld\x1%.2f\x1%d\x1%ld\x1%s",
                    investment->date, investment->amount,  
                    investment->package_id, investment->returns, investment->months_returned,
                    strlen(investment->package), investment->package);
                investment = investment->next;
            }
        }
        
        acc_p = acc_p->next;
    }

    fclose(fout);
    
    //encrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true,false);
            
    status = true;
    dlclose(libjermCrypt);

    return status;

}

bool load_structure(String jermCrypt_path, String save_dir)
{
    /* the *scanf family of functions most important logic
      All three functions(scanf, fscanf, sscanf) read format-string from left to right. 
      Characters outside of conversion specifications are expected to match the sequence of 
      characters in the input stream; the matched characters in the input stream are scanned but not stored. 
      If a character in the input stream conflicts with format-string, the function ends, terminating with 
      a "matching" failure. The conflicting character is left in the input stream as if it had not been read.
        
        - https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.bpxbd00/fscanf.htm

      Also, remember that the []scanf family of functions ALWAYS stop at any whitespace when reading strings
      and as such, in order to read a full string (including the spaces in it) use --regex-- in the %s modifier
      eg %[^\x1]s will scan the entire string(whitespace included) until it meets a \x1(our separation xter)!
      however, even then, you can get a buffer overflow(the scaned string is larger than the char * where its
      meant to go) so to prevent that, use a width specifier eg %50[^\x1]s will snac upto 50 xters 
    */

    bool status = false;

    unsigned int buff_length;
    String crypt_lib_paths[] = {jermCrypt_path,"/",JERM_CRYPT_LIB,"\0"};
    String data_file_paths[] = {save_dir, "/",STRUCTURE_FILE,"\0"};
    
    length_of_all_strings(crypt_lib_paths, &buff_length);
    char crypt_lib_path[buff_length+1];
    join_strings(crypt_lib_path, crypt_lib_paths);

    length_of_all_strings(data_file_paths, &buff_length);
    char data_file_path[buff_length+1];
    join_strings(data_file_path, data_file_paths);

    FILE *fin = fopen(data_file_path,"rb");

    if(!fin) return status;

    /* load function from jermCrypt library eg; */
    void *libjermCrypt = dlopen(crypt_lib_path, RTLD_GLOBAL|RTLD_LAZY);

    if(!libjermCrypt) {fclose(fin); return status;}

    /* encrypt_file & decrypt_file prototypes in libjermCrypt.so */
    // char *encrypt_file(char *fin, char *pswd, char *fout, int overwrite, int verbose);
    // char *decrypt_file(char *fin, char *pswd, char *fout, int overwrite, int verbose);

    char *(*encrypt_file)(char*,char*,char*,int,int) = dlsym(libjermCrypt, "encrypt_file");

    char *(*decrypt_file)(char*,char*,char*,int,int) = dlsym(libjermCrypt, "decrypt_file");

    if(encrypt_file==NULL || decrypt_file==NULL) {dlclose(libjermCrypt); return status;}

    fclose(fin);


    AccountPointer acc_p, p_acc_p; /* p_acc_p = prev ac_p */
    AccountPointer child, p_child; /* p_acc_p = prev ac_p */
    Account acc;
    Commission commission, p_commission, new_commission;
    Investment investment, p_investment, new_investment;

    Child child_p=NULL, last_child_p=NULL, new_child_p, p_child_p;
    unsigned long number_of_items, child_id, string_index, string_length;
    
    /* perform garbage collection on the current structure*/
    if(HEAD->next!=NULL)
    {
        acc_p = HEAD->next;
        gfree(HEAD); /* first clear HEAD after getting the first child it points to */
        
        while(acc_p!=NULL)
        {
            acc = acc_p->account;
            gfree(acc->names);
            
            commission = acc->commissions;
            
            while(commission!=NULL)
            {
                gfree(commission->reason);
                p_commission = commission;
                commission = commission->next;
                gfree(p_commission);
            }
            
            investment = acc->investments;
            while(investment!=NULL)
            {
                gfree(investment->package);
                p_investment = investment;
                investment = investment->next;
                gfree(p_investment);
            }
            
            child = acc->children;
            while(child!=NULL)
            {
                p_child = child;
                child = child->next;
                gfree(p_child);
            }
            
            
            p_acc_p = acc_p;
            acc_p = acc_p->next;
            gfree(acc);
            gfree(p_acc_p);
        }
        
        HEAD = (AccountPointer)malloc(sizeof(struct account_pointer));

        if(HEAD==NULL) memerror(stdout);

        HEAD->account = NULL;
        HEAD->next = NULL;
        HEAD->prev = NULL;

        TAIL = HEAD;
    }

    //decrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true,false);

    fin = fopen(data_file_path,"rb");

    /* load structure */
/*
    AccountPointer acc_p, p_acc_p;
    AccountPointer child, p_child;
    Account acc;
    Commission commission, p_commission;
    Investment investment, p_investment;

    Child child_p=NULL;
    unsigned long number_of_items;
*/    
    ID id, uplink_id;
    while(fscanf(fin, "%ld\x1", &id)!=EOF)
    {
        Account new_account = (Account)malloc(sizeof(struct account));
        
        if(new_account==NULL) memerror(stdout);

        new_account->id = id;
        new_account->names = NULL;
        new_account->pv = 0;
        new_account->available_balance = 0;
        new_account->total_returns = 0;
        new_account->total_redeems = 0;
        
        new_account->uplink = NULL;
        new_account->children = NULL;
        new_account->last_child = NULL;
        new_account->commissions = NULL;
        new_account->last_commission = NULL;

        new_account->leg_volumes[0] = 0.0;
        new_account->leg_volumes[1] = 0.0;
        new_account->leg_volumes[2] = 0.0;

        new_account->TVC_levels[0] = 0;
        new_account->TVC_levels[1] = 0;

        new_account->investments = NULL;
        new_account->last_investment = NULL;

        new_account->rank = 0;

        new_account->highest_leg_ranks[0] = 0.0;
        new_account->highest_leg_ranks[1] = 0.0;
        new_account->highest_leg_ranks[2] = 0.0;

        fscanf(fin,
            "%f\x1"
            "%f\x1"
            "%f\x1"
            "%f\x1"
            "%ld\x1"
            "%f\x1"
            "%f\x1"
            "%f\x1"
            "%f\x1"
            "%f\x1"
            "%d\x1"
            "%d\x1"
            "%d\x1"
            "%d\x1",
            &(new_account->pv), &(new_account->available_balance), &(new_account->total_returns),
            &(new_account->total_redeems), &uplink_id, new_account->leg_volumes,
            &(new_account->leg_volumes[1]), &(new_account->leg_volumes[2]),
            new_account->TVC_levels, &(new_account->TVC_levels[1]), &(new_account->rank),
            new_account->highest_leg_ranks, &(new_account->highest_leg_ranks[1]),
            &(new_account->highest_leg_ranks[2])
        );
        
        new_account->uplink = get_account_by_id(uplink_id);

        string_length = 0;
        string_index = 0;
        fscanf(fin,"%ld\x1",&string_length);

        new_account->names = malloc(sizeof(char)*(string_length+1));
        if(new_account->names==NULL){gfree(new_account); memerror(stdout);}
        while(string_length)
        {
            fscanf(fin,"%c",&(new_account->names[string_index]));
            --string_length;
            ++string_index;
        } new_account->names[string_index]='\0';

        number_of_items = 0;
        fscanf(fin,"%ld\x1",&number_of_items);

        if(number_of_items)
        {
            while(number_of_items)
            {
                fscanf(fin,"%ld\x1",&child_id);
                
                new_child_p = (Child)malloc(sizeof(struct child));
                if(new_child_p==NULL)
                    {gfree(new_account->names); gfree(new_account); memerror(stdout);}
                
                new_child_p->id = child_id;
                new_child_p->uplink_id = id;
                new_child_p->next = NULL;
                
                if(child_p==NULL)
                    child_p = new_child_p;
                else
                    last_child_p->next = new_child_p;
                
                last_child_p = new_child_p;
                
                --number_of_items;
            }
        }

        number_of_items = 0;
        fscanf(fin,"%ld\x1",&number_of_items);

        if(number_of_items)
        {
            while(number_of_items)
            {
                new_commission = (Commission)malloc(sizeof(struct commission));
                if(new_commission==NULL)
                {
                    /* perform garbage collection on all malloc'd data */
                    gfree(new_account->names);
                    /* the created child_pointers (child_p) are left out...LOOK INTO THIS*/
                    gfree(new_account);
                    memerror(stdout);
                }
                
                new_commission->amount = 0;
                new_commission->reason = NULL;
                new_commission->next = NULL;
                new_commission->prev = NULL;
                
                fscanf(fin,"%f\x1",&new_commission->amount);

                string_length = 0;
                string_index = 0;
                fscanf(fin,"%ld\x1",&string_length);

                new_commission->reason = malloc(sizeof(char)*(string_length+1));
                if(new_commission->reason==NULL)
                    {gfree(new_account->names); gfree(new_account); memerror(stdout);}
                
                while(string_length)
                {
                    fscanf(fin,"%c",&(new_commission->reason[string_index]));
                    --string_length;
                    ++string_index;
                } new_commission->reason[string_index]='\0';
                
                if(new_account->commissions==NULL) new_account->commissions=new_commission;
                else
                {
                    new_account->last_commission->next=new_commission;
                    new_commission->prev = new_account->last_commission;
                }
                new_account->last_commission = new_commission;

                --number_of_items;
            }
        }

        number_of_items = 0;
        fscanf(fin,"%ld\x1",&number_of_items);

        if(number_of_items)
        {
            while(number_of_items)
            {
                new_investment = (Investment)malloc(sizeof(struct investment));
                if(new_investment==NULL)
                {
                    /* garbage collection */
                    gfree(new_account->names); 
                    
                    Commission c = new_account->commissions;
                    while(c!=NULL)
                    {
                        gfree(c->reason);
                        c = c->next;
                    }
                    gfree(new_account); 
                    memerror(stdout);
                }

                new_investment->amount = 0;
                new_investment->package = NULL;
                new_investment->package_id = 0;

                new_investment->returns = 0;
                new_investment->months_returned = 0;

                new_investment->next = NULL;
                new_investment->prev = NULL;

                fscanf(fin, "%ld\x1%f\x1%ld\x1%f\x1%d\x1",
                    &(new_investment->date), &(new_investment->amount),  
                    &(new_investment->package_id), &(new_investment->returns), 
                    &(new_investment->months_returned)
                );

                string_length = 0;
                string_index = 0;
                fscanf(fin,"%ld\x1",&string_length);

                new_investment->package = malloc(sizeof(char)*(string_length+1));
                if(new_investment->package==NULL)
                {
                    gfree(new_account->names); 
                    
                    Commission c = new_account->commissions;
                    while(c!=NULL)
                    {
                        gfree(c->reason);
                        p_commission = c;
                        c = c->next;
                        gfree(p_commission);
                    }
                    gfree(new_investment);
                    gfree(new_account); 
                    memerror(stdout);                
                }
                
                while(string_length)
                {
                    fscanf(fin,"%c",&(new_investment->package[string_index]));
                    --string_length;
                    ++string_index;
                } new_investment->package[string_index]='\0';


                if(new_account->investments==NULL) new_account->investments=new_investment;
                else
                {
                    new_account->last_investment->next=new_investment;
                    new_investment->prev = new_account->last_investment;
                }
                new_account->last_investment = new_investment;
                

                
                --number_of_items;
            }
        }

        acc_p = (AccountPointer)malloc(sizeof(struct account_pointer));
        if(acc_p==NULL)
        {
            gfree(new_account->names); 
            
            Commission c = new_account->commissions;
            while(c!=NULL)
            {
                gfree(c->reason);
                p_commission = c;
                c = c->next;
                gfree(p_commission);
            }
            Investment inv = new_account->investments;
            while(inv!=NULL)
            {
                gfree(inv->package);
                p_investment = inv;
                inv = inv->next;
                gfree(p_investment);
            }
            gfree(new_account); 
            memerror(stdout);                
        }
        acc_p->account = new_account;
        acc_p->next=NULL;
        acc_p->prev=NULL;

        if(HEAD->next==NULL) HEAD->next=acc_p;
        else
        {
            TAIL->next=acc_p;
            acc_p->prev=TAIL;
        }
        
        TAIL = acc_p;

        CURRENT_ID = id; /* the current ID is based on saved data!*/
    }

    /* now that all accounts have been loaded, we can properly allocate direct children */
    Account uplink_account, child_account;
    while(child_p!=NULL)
    {
        uplink_account = get_account_by_id(child_p->uplink_id);
        child_account = get_account_by_id(child_p->id);
        
        acc_p = (AccountPointer)malloc(sizeof(struct account_pointer));
        if(acc_p==NULL)
        {
            /* congs, we just robbed an uplink of their direct child. this destroys everything
               from destroying leg volumes to robbing people of the FSB n all kinds of 
               commissions :) :) :) :)
            */
            memerror(stdout);
        }
        
        acc_p->account = child_account;
        acc_p->next = NULL;
        acc_p->prev = NULL;
        
        if(uplink_account->children==NULL) uplink_account->children=acc_p;
        else
        {
            uplink_account->last_child->next=acc_p;
            acc_p->prev=uplink_account->last_child;
        }
        
        uplink_account->last_child = acc_p;

        p_child_p = child_p;
        child_p = child_p->next;
        gfree(p_child_p);
    }

    fclose(fin);
    
    //encrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true,false);
            
    status = true;
    dlclose(libjermCrypt);

    return status;
}
