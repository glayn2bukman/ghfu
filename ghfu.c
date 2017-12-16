/* export this file as a library with
    $ gcc -shared -fPIC -o libjermGHFU.so ghfu.c

    NB: through-out this code, the only time output is written to a file (maybe stdout but nt likely) is
        is when an error occured somewhere. in fact, this and the fact that most functions return 
        true/false is what the other programs interacting with this code use to tell if something went 
        wrong. ie;

            function_reply = libjermGHFU->(void *)func(..., output_logfile)
            if not function_reply:
                error = output_logfile.contents

        the only exception to this rule is function <dump_structure_details> as the sole purpose of this
        function is to actually write the structure details to a json file!


    for functions that return bool, if return value is false and fout is empty, fout coult not be created!
*/

#include "ghfu.h"
#include "data.h"

/* function definitions */

void memerror(FILE *fout)
{
    /* memerror causes the program to exit immediately. however, if this aint the case, please
       ensure that all possible memerror's dont leave any loacked mutex locked before the function
       calling memerror returns otherwise you'll get a freaking hanging program!!!!!
    */
    ghfu_warn(0,fout);

    /* dump all data to file here (dont overwrite files, always create new files everytime you dump)*/

    exit(1); /* is exiting the right choice? if so, please first dump all data to file*/
}

void init(String jermCrypt_path, String save_dir)
{
    /* call at stystem start */

    if(GLOCK_INITIALISED)
        /* prevent a re-calling of init */
    {
        fprintf(stdout, "init already called...skipping"); 
        return;
    }
    if(pthread_mutex_init(&glock, NULL))
     /* init utex to not only work btn threads but processes*/
    {
        fprintf(stdout, "FAILED TO CREATE MUTEX! EXITTING LIBRARY"); 
        exit(2);
    }

    GLOCK_INITIALISED = true;

    /* check if libjermCrypt.so is present*/
    // #######################
    unsigned int buff_length;
    String crypt_lib_paths[] = {jermCrypt_path,"/",JERM_CRYPT_LIB,"\0"};
    
    length_of_all_strings(crypt_lib_paths, &buff_length);
    char crypt_lib_path[buff_length+1];
    join_strings(crypt_lib_path, crypt_lib_paths);

    void *libjermCrypt = dlopen(crypt_lib_path, RTLD_GLOBAL|RTLD_LAZY);

    if(!libjermCrypt) {fprintf(stdout,"libjermCrypt.so not found. exitting system\n"); exit(4);}
    // #######################

    /* check if jermCrypt password file exists */
    FILE *jermCrypt_pswd_file = fopen(JERM_CRYPT_PASSWORD_FILE,"rb");
    if(!jermCrypt_pswd_file)
        {fprintf(stdout,"libjermCrypt password file not found. exitting system\n"); exit(4);}

    unsigned char c;
    unsigned int pswd_xter_pos = 0;
    while ((c=fgetc(jermCrypt_pswd_file)))
    {
        if (feof(jermCrypt_pswd_file)) break;
        JERM_CRYPT_PASSWORD[pswd_xter_pos] = c;
        ++pswd_xter_pos;
    } JERM_CRYPT_PASSWORD[pswd_xter_pos] = '\0';
    
    fclose(jermCrypt_pswd_file);
    // ###############################

    bool data_loaded;

    data_loaded = load_constants(jermCrypt_path, save_dir);
    
    if (!data_loaded && DATA_FILE_PRESENT)
    {
        fprintf(stdout,"data file found but jermCrypt returned error. exitting system\n");
        exit(3);
    }
    
    if(!data_loaded)
    {
        /* these data variables shall be reset when reading data from file */
        SYSTEM_FLOAT=0; CUMULATIVE_COMMISSIONS=0; COMMISSIONS=0;
        
        ACTIVE_ACCOUNTS=0; /* decremeneted when account is deleted */

        CURRENT_ID = 0; /* only increments, never the opposite */
        CURRENT_SERVICE_ID = 0;
        AVAILABLE_INVESTMENTS = 100; /* be carefull of this value. this breaks or makes us! */

        /* monthly-aut-refills 
            default values;
                {
                    {175, 40},
                    {125, 40},
                    {75, 40},
                    {0, 0}
                };

        */
        MONTHLY_AUTO_REFILL_PERCENTAGES = malloc(4*sizeof(float*));
        if (!MONTHLY_AUTO_REFILL_PERCENTAGES) memerror(stdout);
        for(int i=0; i<4; ++i)
        {
            /* any malloc failure in here will lead into memory leaks IF memerror doesn't exit the system */
            MONTHLY_AUTO_REFILL_PERCENTAGES[i] = malloc(sizeof(float)*2);
            if(!MONTHLY_AUTO_REFILL_PERCENTAGES[i]) memerror(stdout);
            switch(i)
            {
                case 0: 
                    {
                        MONTHLY_AUTO_REFILL_PERCENTAGES[i][0] = 175; 
                        MONTHLY_AUTO_REFILL_PERCENTAGES[i][1] = 40; 
                        break;
                    }
                case 1: 
                    {
                        MONTHLY_AUTO_REFILL_PERCENTAGES[i][0] = 125; 
                        MONTHLY_AUTO_REFILL_PERCENTAGES[i][1] = 40; 
                        break;
                    }
                case 2: 
                    {
                        MONTHLY_AUTO_REFILL_PERCENTAGES[i][0] = 75; 
                        MONTHLY_AUTO_REFILL_PERCENTAGES[i][1] = 40; 
                        break;
                    }
                case 3: 
                    {
                        MONTHLY_AUTO_REFILL_PERCENTAGES[i][0] = 0; 
                        MONTHLY_AUTO_REFILL_PERCENTAGES[i][1] = 0; 
                        break;
                    }
            }
        }
    }

    HEAD = malloc(sizeof(struct account_pointer));


    if(!HEAD) memerror(stdout);

    HEAD->account = NULL;
    HEAD->next = NULL;
    HEAD->prev = NULL;

    TAIL = HEAD;

    data_loaded = load_structure(jermCrypt_path, save_dir);

    if (!data_loaded && DATA_FILE_PRESENT)
    {
        fprintf(stdout,"data file found but jermCrypt returned error. exitting system\n");
        exit(3);
    }

    // initiate paths to be used by auto-save...
    GPATHS = malloc(sizeof(struct gpath));
    if(!GPATHS) memerror(stdout);

    String jcp_strings[] = {jermCrypt_path, "\0"}, dfp_strings[]={save_dir,"\0"};
    length_of_all_strings(jcp_strings, &buff_length);

    GPATHS->jerm_crypt_path = malloc(sizeof(char)*(buff_length+1));
    if(!(GPATHS->jerm_crypt_path)) {gfree(GPATHS); memerror(stdout);}
    join_strings(GPATHS->jerm_crypt_path,jcp_strings);


    length_of_all_strings(dfp_strings, &buff_length);

    GPATHS->data_files_path = malloc(sizeof(char)*(buff_length+1));
    if(!(GPATHS->data_files_path)) {gfree(GPATHS->jerm_crypt_path); gfree(GPATHS); memerror(stdout);}
    join_strings(GPATHS->data_files_path,dfp_strings);

}

bool increment_pv(Account account, const Amount points, FILE *fout)
{
    /* this function ensures that all PV increments anss rank updates in a certain line of sucession
       occur are updated all at one when a PV changes. this in turn means that the web-API that calls
       for information never delays because the data is not calculated/determined at calling time but
       rather is updated and made readily available any time the web-API needs it 
    */

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO increment PV. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("I-PV\n");

    if(!account){ghfu_warn(7,fout); return false;}

    time_t t; t = time(NULL);

    pthread_mutex_lock(&glock);    
    account->pv += points;
    
    if ((t-account->date)<=GHFU_MONTH) 
        account->pv_made_in_first_month += points;
    
    pthread_mutex_unlock(&glock);    

    raise_rank(account, fout);

    /* now update the appropriate leg CV for all uplinks until ROOT(uplink==NULL) in the tree of this account 
       this dramatically reduces the time needed to calculate the TVCs and all other TEAM-volume related 
       calculations as the values will be readily available when needed

       also, increment ranks for all liable members in that linear tree!
    */

    Account acc = account->uplink;

    unsigned int highest_rank, i;
    
    while(acc)
    {
        raise_rank(acc, fout);

        pthread_mutex_lock(&glock);

        highest_rank = acc->highest_leg_ranks[0];
        for(i=1;i<3;++i)
            acc->highest_leg_ranks[i]>highest_rank ? highest_rank=acc->highest_leg_ranks[i] : 0;

        if(acc->uplink)
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

        pthread_mutex_unlock(&glock);

    }
    
    return true;
}

bool award_commission(Account account, Amount points, String commission_type, String reason, FILE *fout)
{
    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO award commission. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("AC\n");

    if(!points){ghfu_warn(8,fout); return false;}
    if(points<0) {ghfu_warn(36,fout); return false;}
    if(!account) {fprintf(fout,"could not award commissions for NULL account!\n"); return false;}
    
    Commission new_commission = malloc(sizeof(struct commission));
    if(!new_commission) memerror(fout);

    new_commission->date = time(NULL);

    new_commission->reason = NULL;
    new_commission->amount = 0;

    new_commission->next = NULL;
    new_commission->prev = NULL;

    Amount p_comm=0;
    char points_str[16];
    unsigned int buff_length;
    
    bool update_pv = false;
    
    if(!strcmp(commission_type,"QSB"))
    {
        /* fast start bonus (whenver a new member joins)*/
        int i=0;
        pthread_mutex_lock(&glock);
        for(; FSB[i][1]; ++i)
        {   
            if(account->pv < FSB[i+1][0]) break;
        }
        pthread_mutex_unlock(&glock);

        FSB[i][1] ? i : --i; /* could have brocken up from the {0,0} terminating array*/

        p_comm = (FSB[i][1]/100)*points;

        //update_pv = true;        
        
    }
    else if(!strcmp(commission_type,"IBC"))
    {
        /* independent brocker commission (whenever a package is bought) */
        int i=0;
        pthread_mutex_lock(&glock);
        for(; IBC[i][1]; ++i)
        {
            if(account->pv < IBC[i+1][0]) break;
        }
        pthread_mutex_unlock(&glock);

        IBC[i][1] ? i : --i; /* could have brocken up from the {0,0} terminating array*/
        
        p_comm = (IBC[i][1]/100)*points;
        
        //update_pv = true;        
    }
    else if(!strcmp(commission_type, "REBET")) {p_comm = points;}
    else if(!strcmp(commission_type, "DRA")) {p_comm = points;} /*director's recorgnition award*/    
    else if(!strcmp(commission_type, "HOB")) {p_comm = HOB*0.01*points;}    
    else if(!strcmp(commission_type, "LCB")) {p_comm = LCB*0.01*points;}    
    else if(!strcmp(commission_type, "EAB")) {p_comm = EAB*0.01*points;}    
    else if(!strcmp(commission_type, "TBB"))
    {
        /* team-building-bonus (whenever a new member joins)*/
        
        gfree(new_commission); /* you dont want memory leaks, trust me */
        
        if(!(account->uplink)) return false;
        
        pthread_mutex_lock(&glock);
        
        int generation = 1;
        Account uplink = account->uplink->uplink;
        
        Commission last_commission;
        
        while((generation<=TBB_MAX_GENERATIONS) && (uplink))
        /* the 2 conditions are arranged in that order for a reason; when we have a sufficiently large 
           structure, its mch more likely that we shall reach the generation<=7 barrier of TBBs before
           we reach the root node for that given lineage!
        */
        {
            int i=0;
            for(; TBB[i][1]; ++i)
            {
                if(uplink->pv < TBB[i+1][0]) break;
            }
            
            TBB[i][1] ? i : --i; /* could have brocken up from the {0,0} terminating array*/
            
            p_comm = (TBB[i][generation]/100)*points;
            
            if(p_comm)
            {
                last_commission = uplink->last_commission;

                uplink->last_commission = malloc(sizeof(struct commission));
                if(!(uplink->last_commission)) memerror(fout);

                uplink->last_commission->date = time(NULL);

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
                uplink->last_commission->reason = malloc(sizeof(char)*(buff_length+1));
                if(!(uplink->last_commission->reason)) memerror(fout);
                join_strings(uplink->last_commission->reason, reason_strings);
                
                if(!(uplink->commissions)){ uplink->commissions = uplink->last_commission; }
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

        pthread_mutex_unlock(&glock);

    }

    if(p_comm)
    {
        sprintf(points_str," ($%.2f)",p_comm);
        String reason_strings[] = {reason, points_str, "\0"};
        length_of_all_strings(reason_strings, &buff_length);

        /* create a new array to hold this commission reason (you cant just assign a local char[] to
           new_commission->reason...basically U DONT WANNA ASSIGN TO A LOCAL POINTER BCOZ ONCE D FUCTION
           EXITS, THE POINTER WILL BE ANONYMOUS!)*/
        new_commission->reason = malloc(sizeof(char)*(buff_length+1));
        if(!(new_commission->reason)) memerror(fout); /* could do much better to rollback all changes 
                                                            but i dint coz memerror exits the program!*/
        join_strings(new_commission->reason,reason_strings);

        pthread_mutex_lock(&glock);

        new_commission->amount = p_comm;
        account->available_balance += p_comm;
        account->total_returns += p_comm;

        if(!(account->commissions))
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

        pthread_mutex_unlock(&glock);

        if(update_pv) increment_pv(account,points, fout);


        pthread_mutex_lock(&glock);

        CUMULATIVE_COMMISSIONS += p_comm;
        COMMISSIONS += p_comm;
        
        pthread_mutex_unlock(&glock);

    }

    return true;
}

bool invest(ID account_id, const Amount amount, const bool update_system_float, bool test_feasibility, String fout_name)
{
    /*python/java/etc interface to invest_money*/

    FILE *fout = fopen(fout_name, "w");
    if(!fout) return false;

    Account account = get_account_by_id(account_id);

    bool invested = invest_money(account,amount, update_system_float, test_feasibility, fout);

    fclose(fout);
    
    return invested;
}

bool invest_money(Account account, Amount amount, bool update_system_float, bool test_feasibility, FILE *fout)
{
    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO invest money. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("I-Money\n");

    if(amount<0) {ghfu_warn(36,fout); return false;}

    time_t t; t = time(NULL);
    struct tm *today = localtime(&t);

    if(today->tm_mday > LAST_INVESTMENT_DAY){ghfu_warn(19,fout); return false;}

    if(!AVAILABLE_INVESTMENTS){ghfu_warn(25,fout); return false;}


    if(!account) 
        {fprintf(fout,"could not create investment. Invalid account provided!\n"); return false;}
    if (account->is_consumer){ghfu_warn(27,fout); return false;}

    // nelson insisted that one can not make more than 1 investments in a year!
    if (account->investments && account->last_investment->months_returned!=12)
        {fprintf(fout, "cant make more than one investment in 12 months\n"); return false;}


    amount -= OPERATIONS_FEE;
    
    pthread_mutex_lock(&glock);
    
    if(amount<0)
    { 
        printf("failed to invest for <%s>...", account->names); ghfu_warn(4,fout); 

        pthread_mutex_unlock(&glock);

        return false; 
    }
    if(!amount)
    { 
        printf("failed to invest for <%s>...", account->names); ghfu_warn(5,fout); 

        pthread_mutex_unlock(&glock);

        return false; 
    }
    if(amount<MINIMUM_INVESTMENT)
    { 
        printf("failed to invest for <%s>...", account->names); ghfu_warn(6,fout); 

        pthread_mutex_unlock(&glock);

        return false; 
    }
    if(amount>MAXIMUM_INVESTMENT)
    { 
        printf("failed to invest for <%s>...", account->names); ghfu_warn(9,fout); 

        pthread_mutex_unlock(&glock);

        return false; 
    }
    /* lastly, check if the investment is descrete */
    /* also, generate the package name and ID from here if possible! 2 birds, 1 stone...*/
    String package; ID package_id, package_count=0;
    for(;; ++package_count)
    {
        if (!INVESTMENTS[package_count])
            {
                ghfu_warn(18, fout);
                pthread_mutex_unlock(&glock);
                return false;            
            }
        if(amount==INVESTMENTS[package_count])
        {
            package_id = package_count;
            package = ACCOUNTS[++package_count];
            break;
        }
    }

    if (test_feasibility) /* no more checks, operation is feasible*/ 
    {
        pthread_mutex_unlock(&glock);
        return true; 
    }

    Investment new_investment = malloc(sizeof(struct investment));
    
    if(!new_investment) memerror(fout);

    Amount points = amount*POINT_FACTOR;
        
    
    new_investment->date = time(NULL);

    /* we dont know that package will point to after we exit here so we create space for it on the heap 
       i witnessed this issue when calling libjermGHFU.so->invest from python. see that the commission
       reason in award_commission does the same
    */

    new_investment->amount = points;
    new_investment->package = NULL;
    new_investment->package_id = package_id;

    new_investment->returns = 0;
    new_investment->months_returned = 0;

    new_investment->total_returns_on_creation = account->total_returns;
    new_investment->skipped_first_payment_day = false;

    new_investment->next = NULL;
    new_investment->prev = NULL;

    unsigned int buff_length;
    String reason_strings[] = {package, "\0"};
    length_of_all_strings(reason_strings, &buff_length);

    new_investment->package = malloc(sizeof(char)*(buff_length+1));
    if(!(new_investment->package)) {gfree(new_investment); memerror(fout);}
    join_strings(new_investment->package,reason_strings);

    if(!(account->investments))
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

    pthread_mutex_unlock(&glock);


    if (account->uplink)
    {
        String c_reason_strings[] = {"QSB from ", account->names,"'s investment", "\0"};
        length_of_all_strings(c_reason_strings, &buff_length);
        char c_reason[buff_length+1];
        join_strings(c_reason, c_reason_strings);
        award_commission(account->uplink,points,"QSB",c_reason, fout);
    }

    if(update_system_float)
    {   
        pthread_mutex_lock(&glock);

        SYSTEM_FLOAT += amount;

        pthread_mutex_unlock(&glock);
    }

    increment_pv(account,points,fout);

    pthread_mutex_lock(&glock);

    --AVAILABLE_INVESTMENTS;

    pthread_mutex_unlock(&glock);


    return true; /* leave this out and you have memory leaks when invest money is called from regiter_member*/
}

ID register_new_member(ID uplink_id, String names, Amount amount, bool test_feasibility, String fout_name)
{
    /* if return value==0, error occured, otherwise, new account ID is returned */

    /* python/java/etc interface to register_member */
    FILE *fout = fopen(fout_name,"a");    
    if(!fout) return 0;

    ID new_id;

    if(uplink_id && !get_account_by_id(uplink_id)){fprintf(fout, "uplink does not exist!"); new_id=0;}
    else
        new_id = account_id(register_member(get_account_by_id(uplink_id), names, amount, test_feasibility, fout));
    
    fclose(fout);
    
    return new_id;
}

Account register_member(Account uplink, String names, Amount amount, bool test_feasibility, FILE *fout)
{
    /* if this function returns NULL, DONT USE RESULT (it indicates an error, more like malloc)*/

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO register member. glock NOT INITIALISED. did you call init?"); 
        return NULL;
    }

    //printf("Reg Member\n");

    if(amount<0) {ghfu_warn(36,fout); return NULL;}
    
    bool is_consumer = amount ? false : true;
    if (is_consumer && (!HEAD->next)){ghfu_warn(26,fout); return NULL;}
    if(uplink && uplink->is_consumer){ghfu_warn(29,fout); return NULL;}

    if (!is_consumer)
    {
        if(amount<(ACCOUNT_CREATION_FEE))
            { fprintf(fout, "failed to add <%s>...",names); ghfu_warn(1,fout); return NULL; }
        if(amount>(ACCOUNT_CREATION_FEE + MAXIMUM_INVESTMENT+OPERATIONS_FEE))
            { fprintf(fout, "failed to add <%s>...",names); ghfu_warn(9,fout); return NULL; }
    }

    Amount _amount, points;
    
    if (is_consumer) {_amount=0; points=0;}
    else
    {
        _amount = amount;
        amount -= ACCOUNT_CREATION_FEE;
        
        /* turn investable amount to points */
        points=amount*POINT_FACTOR;
    }

    Account new_account = malloc(sizeof(struct account));
    
    if(!new_account) memerror(fout);

    pthread_mutex_lock(&glock);
    new_account->id = CURRENT_ID+1;
    pthread_mutex_unlock(&glock);

    new_account->is_consumer = is_consumer;

    new_account->names = NULL;
    new_account->pv = 0;
    new_account->available_balance = 0;
    new_account->total_returns = 0;
    new_account->total_redeems = 0;
    new_account->redeems = 0;
    
    new_account->uplink = is_consumer ? (Account)(HEAD->next->account) : uplink;
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

    new_account->withdraws = NULL;
    new_account->last_withdraw = NULL;

    new_account->services = NULL;
    new_account->last_service = NULL;

    new_account->rank = 0;

    new_account->highest_leg_ranks[0] = 0.0;
    new_account->highest_leg_ranks[1] = 0.0;
    new_account->highest_leg_ranks[2] = 0.0;

    new_account->pv_made_in_first_month = 0.0;

    new_account->date = time(NULL);

    if (test_feasibility) /* no more checks, operation is feasible*/
    {
        if(points>0)
            invest_money(new_account, amount, false, test_feasibility, fout);
        
        gfree(new_account); /* you dont want memory leaks. trust me on this */
        
        return NULL; /*since register doesnt return a bool, check if <fout> has contents to determine if
                       an error occurered anywhere...*/
    }

    unsigned int buff_length;
    String name_strings[] = {names, "\0"};
    length_of_all_strings(name_strings, &buff_length);

    new_account->names = malloc(sizeof(char)*(buff_length+1));
    if(!(new_account->names)) {gfree(new_account); memerror(fout);}
    join_strings(new_account->names,name_strings);

    
    /* add new_account to uplink's children and then give uplink FSB */
    AccountPointer ap1 = malloc(sizeof(struct account_pointer));

    if(uplink || is_consumer)
    {
        /* add the new account to uplink's children */
        AccountPointer ap = ap1;
        if(!ap)
        {
            gfree(new_account->names);
            gfree(new_account); /* you dont want memory leaks, trust me */
            memerror(fout);
        }
        
        ap->account = new_account;
        ap->next = NULL;
        ap->prev = NULL;

        pthread_mutex_lock(&glock);
        
        if (!is_consumer)
        {
            if(!(uplink->children)) uplink->children = ap;
            else { ap->prev = uplink->last_child; uplink->last_child->next = ap;}

            uplink->last_child = ap;
        }
        else
        {
            if(!(new_account->uplink->children)) new_account->uplink->children = ap;
            else { ap->prev = new_account->uplink->last_child; new_account->uplink->last_child->next = ap;}

            new_account->uplink->last_child = ap;
        }
        pthread_mutex_unlock(&glock);
        
    }
    else {gfree(ap1); /* you dont want memory leaks, trust me */ }

    /* add new node to linear structure of all nodes */
    AccountPointer ap = malloc(sizeof(struct account_pointer));
    if(!ap)
    {
         /* you dont want memory leaks, trust me */
        gfree(new_account->names);
        if(uplink) gfree(ap1);
        gfree(new_account);
        
        memerror(fout);
    }

    /* attempt to invest money (after declaring that new_member is a child of uplink; this is important
       because invest_money calls incrememt_pv which needs to call children of uplinks)*/
    if(points>0 && !invest_money(new_account, amount, false, test_feasibility, fout))
    {
        if(uplink)
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
    if(points>0) award_commission(new_account, (amount-OPERATIONS_FEE)*POINT_FACTOR,"TBB","", fout);

    ap->account = new_account;
    ap->next = NULL;
    ap->prev = NULL;

    pthread_mutex_lock(&glock);

    if(!(HEAD->next)) { ap->prev = HEAD; HEAD->next = ap; }
    else { ap->prev = TAIL; TAIL->next = ap; }
    
    TAIL = ap;

    /* update system float */
    SYSTEM_FLOAT += _amount;

    ++CURRENT_ID; ++ACTIVE_ACCOUNTS;

    pthread_mutex_unlock(&glock);

    return new_account;
}

bool buy_property(Account IB_account, Amount amount, bool test_feasibility, FILE *fout)
{
    /* if member(IV or registered investor), get IBC and those points otherwise just add this amount to 
       the system total float
    
        NB: This function takes amount in USD but in actual sence, the property is sold in UGX. The
            value provided as amount here is thus actual_amount(UGX)/FIXED_PROPERTY_EXCHANGE_RATE. This
            if left to whatever interface is communicating with this code eg a python/java/c++/etc server
    */

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO buy property. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("buy-property\n");

    if(!amount){ghfu_warn(17,fout); return false;}
    if(amount<0) {ghfu_warn(36,fout); return false;}

    if (test_feasibility) return true; /* no more checks, operation is feasible*/

    Amount points = amount*POINT_FACTOR;

    if(IB_account)
    {
        if (!(IB_account->is_consumer))
        {                
            /* create reason for the commission */
            char points_str[25];
            sprintf(points_str," %.2f",points);
            
            String reason_strings[] = {"IBC from buying yourself a service/package worth",points_str," points","\0"};
            unsigned int buff_length;
            length_of_all_strings(reason_strings, &buff_length);
            char reason[buff_length+1];
            join_strings(reason,reason_strings);

            award_commission(IB_account, points, "IBC", reason,fout);

            if (IB_account->uplink)
            {
                String c_reason_strings[] = {"QSB from ", IB_account->names,"'s purchase of a package worth", points_str," points","\0"};
                length_of_all_strings(c_reason_strings, &buff_length);
                char c_reason[buff_length+1];
                join_strings(c_reason, c_reason_strings);
                award_commission(IB_account->uplink,points,"QSB",c_reason, fout);
            }

            award_commission(IB_account, points,"TBB","", fout);
        }
        
        increment_pv(IB_account,points,fout);
    }

    pthread_mutex_lock(&glock);
    
    SYSTEM_FLOAT += amount;
    
    pthread_mutex_unlock(&glock);

    return true;
}

bool purchase_property(ID IB_account_id, const Amount amount, bool test_feasibility, String fout_name)
{
    /* python/java/etc interface to buy_property*/

    FILE *fout = fopen(fout_name, "w");
    if(!fout) return false;

    Account IB_account = get_account_by_id(IB_account_id);
    
    bool status = buy_property(IB_account,amount, test_feasibility, fout);
    
    fclose(fout);

    return status;
}

bool auto_refill(Account account, FILE *fout)
{
    
    /* if account==NULL, calculate auto-refills for all investments in the system otherwise, 
       calculate auto-refill for only that account's investments 
    */

    /* this function is called once every month (on an agreed date) */

    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO do auto-refill. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    for(unsigned int _c=0; MONTHLY_AUTO_REFILL_PERCENTAGES[_c][0]; ++_c)
    {
        if(MONTHLY_AUTO_REFILL_PERCENTAGES[_c][0]<0 || MONTHLY_AUTO_REFILL_PERCENTAGES[_c][1]<0) 
            {ghfu_warn(36,fout); return false;}
    }


    //printf("AR\n");

    time_t t; t = time(NULL);
    struct tm *today = localtime(&t);

    if(today->tm_mday!=PAYMENT_DAY){ghfu_warn(12,fout); return false;}

    if(!MONTHLY_AUTO_REFILL_PERCENTAGES){ghfu_warn(16,fout); return false;}
        
    Commission new_commission;
    Investment inv;
    float min_accaptable_returns, max_accaptable_returns, accumulated_returns_since_investment;
    unsigned int i, buff_length, scenario; /*
        scenario cases;
         0) months_returned for investment < 12
         1) total_returns since investment < investment + max profit possible.
         2) total_returns since investment + this month's return > investment + max profit possible
         3) total_returns since investment + 12'th month's return < investment + min profit possible
         
         ie;
         check investment->skipped_first_payment_day is true. if not, set to true and move on to next
            investment

         check for condition 0 and only proceed if its met then... 
         first check for condition 1. If it passes, calculate the new return and check for
         condition 2. If condition 2 fails, let the returns be whatever is missing to top the total
         returns to total_returns since investment + max-profit possible.
         If its the 11'th month(before u increment it to 12'th), then check for condition 3. Th condition
         checks for 11'th or 12'th month depending on if months_returned is incremented before or after
         these checks
         
                                            */
    Amount returns;

    char returns_str[16], points_str[32], active_percentage_str[16];
    String reason_strings[10];                        

    if(account)
    {
        if(!(account->is_consumer) && (account->investments))
        {
           pthread_mutex_lock(&glock); 
           inv = account->investments;
           while(inv)
           {
               if(inv->skipped_first_payment_day && inv->months_returned<12)
               {
                    i = 0;
                    for(; MONTHLY_AUTO_REFILL_PERCENTAGES[i][0]; ++i)
                    {
                        if(inv->amount >= MONTHLY_AUTO_REFILL_PERCENTAGES[i][0]) break;
                    }
                    
                    returns = (inv->amount)*(MONTHLY_AUTO_REFILL_PERCENTAGES[i][1])*.01;

                    min_accaptable_returns = ((inv->amount/POINT_FACTOR)+OPERATIONS_FEE)*(100+INVESTMEN_SCHEME[i][3])*.01;
                    max_accaptable_returns = ((inv->amount/POINT_FACTOR)+OPERATIONS_FEE)*(100+INVESTMEN_SCHEME[i][4])*.01;
                                        
                    accumulated_returns_since_investment = account->total_returns - inv->total_returns_on_creation;

                    // here, scenario represents the error to reflect
                    if (accumulated_returns_since_investment>=max_accaptable_returns)
                    {
                        scenario = 21; 
                        returns=0;
                    }
                    else if (accumulated_returns_since_investment + returns > max_accaptable_returns)
                    {
                        scenario = 22;
                        returns = max_accaptable_returns-accumulated_returns_since_investment;
                    }
                    else if ( (inv->months_returned==11) &&
                        (accumulated_returns_since_investment + returns < min_accaptable_returns) )
                    {
                        scenario = 23;
                        returns = min_accaptable_returns-accumulated_returns_since_investment;
                    }
                    else
                        scenario = 0; // normal scenario

                    if(!MONTHLY_AUTO_REFILL_PERCENTAGES[i][0]) ghfu_warn(13,fout);
                    else
                    {
                        new_commission = malloc(sizeof(struct commission));
                        if(!new_commission) memerror(fout);

                        new_commission->date = time(NULL);

                        new_commission->reason = NULL;
                        new_commission->amount = returns;

                        new_commission->next = NULL;
                        new_commission->prev = NULL;

                        if(!(account->commissions))
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

                        if (!scenario)
                        {
                            sprintf(points_str," worth %.2f points",inv->amount);
                            sprintf(active_percentage_str," %.2f%%",MONTHLY_AUTO_REFILL_PERCENTAGES[i][1]);

                            reason_strings[0] = "Investment return for package <";
                            reason_strings[1] = inv->package;
                            reason_strings[2] = "> ";
                            reason_strings[3] = points_str;
                            reason_strings[4] = returns_str;
                            reason_strings[5] = " given at";
                            reason_strings[6] = active_percentage_str;
                            reason_strings[7] = "\0";
                                                     }    
                        else
                        {
                            reason_strings[0] = ERRORS[scenario]; 
                            reason_strings[1] = returns_str;
                            reason_strings[2] = "\0";                        
                        }
                        
                        length_of_all_strings(reason_strings, &buff_length);

                        /* create a new array to hold this commission reason (you cant just assign a local char[] to
                           new_commission->reason...basically U DONT WANNA ASSIGN TO A LOCAL POINTER BCOZ ONCE D FUCTION
                           EXITS, THE POINTER WILL BE ANONYMOUS!)*/
                        new_commission->reason = malloc(sizeof(char)*(buff_length+1));
                        if(!(new_commission->reason)) {gfree(new_commission); memerror(fout);}
                        join_strings(new_commission->reason,reason_strings);

                        ++(inv->months_returned);
                        inv->returns += returns;
                        account->available_balance += returns;
                        account->total_returns += returns;
                        
                        CUMULATIVE_COMMISSIONS += returns;
                        COMMISSIONS += returns;
                        
                    }
                   
               }

               if (!(inv->skipped_first_payment_day))
                    inv->skipped_first_payment_day = true;

               inv = inv->next;
           }
           
           pthread_mutex_unlock(&glock); 
 
        }
        return true;
    }

    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = !acc_p ? acc_p : acc_p->next;

    while(acc_p)
    {
        acc = acc_p->account;

        if(!(acc->is_consumer) && (acc->investments))
        {

           pthread_mutex_lock(&glock); 
           inv = acc->investments;
           while(inv)
           {
               if(inv->skipped_first_payment_day && inv->months_returned<12)
               {
                    i = 0;
                    for(; MONTHLY_AUTO_REFILL_PERCENTAGES[i][0]; ++i)
                    {
                        if(inv->amount >= MONTHLY_AUTO_REFILL_PERCENTAGES[i][0]) break;
                    }
                    
                    returns = (inv->amount)*(MONTHLY_AUTO_REFILL_PERCENTAGES[i][1])*.01;

                    min_accaptable_returns = ((inv->amount/POINT_FACTOR)+OPERATIONS_FEE)*(100+INVESTMEN_SCHEME[i][3])*.01;
                    max_accaptable_returns = ((inv->amount/POINT_FACTOR)+OPERATIONS_FEE)*(100+INVESTMEN_SCHEME[i][4])*.01;
                    accumulated_returns_since_investment = acc->total_returns - inv->total_returns_on_creation;

                    // here, scenario represents the error to reflect
                    if (accumulated_returns_since_investment>=max_accaptable_returns)
                    {
                        scenario = 21; 
                        returns=0;
                    }
                    else if (accumulated_returns_since_investment + returns > max_accaptable_returns)
                    {
                        scenario = 22;
                        returns = max_accaptable_returns-accumulated_returns_since_investment;
                    }
                    else if ( (inv->months_returned==11) &&
                        (accumulated_returns_since_investment + returns < min_accaptable_returns) )
                    {
                        scenario = 23;
                        returns = min_accaptable_returns-accumulated_returns_since_investment;
                    }
                    else
                        scenario = 0; // normal scenario

                    if(!MONTHLY_AUTO_REFILL_PERCENTAGES[i][0]) ghfu_warn(13,fout);
                    else
                    {
                        new_commission = malloc(sizeof(struct commission));
                        if(!new_commission) memerror(fout);

                        new_commission->date = time(NULL);

                        new_commission->reason = NULL;
                        new_commission->amount = returns;

                        new_commission->next = NULL;
                        new_commission->prev = NULL;

                        if(!(acc->commissions))
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

                        if (!scenario)
                        {
                            sprintf(points_str," worth %.2f points",inv->amount);
                            sprintf(active_percentage_str," %.2f%%",MONTHLY_AUTO_REFILL_PERCENTAGES[i][1]);

                            reason_strings[0] = "Investment return for package <";
                            reason_strings[1] = inv->package;
                            reason_strings[2] = "> ";
                            reason_strings[3] = points_str;
                            reason_strings[4] = returns_str;
                            reason_strings[5] = " given at";
                            reason_strings[6] = active_percentage_str;
                            reason_strings[7] = "\0";
                                                     }    
                        else
                        {
                            reason_strings[0] = ERRORS[scenario]; 
                            reason_strings[1] = returns_str;
                            reason_strings[2] = "\0";                        
                        }

                        length_of_all_strings(reason_strings, &buff_length);

                        /* create a new array to hold this commission reason (you cant just assign a local char[] to
                           new_commission->reason...basically U DONT WANNA ASSIGN TO A LOCAL POINTER BCOZ ONCE D FUCTION
                           EXITS, THE POINTER WILL BE ANONYMOUS!)*/
                        new_commission->reason = malloc(sizeof(char)*(buff_length+1));
                        if(!(new_commission->reason)) {gfree(new_commission); memerror(fout);}
                        join_strings(new_commission->reason,reason_strings);

                        ++(inv->months_returned);
                        inv->returns += returns;
                        acc->available_balance += returns;
                        acc->total_returns += returns;

                        CUMULATIVE_COMMISSIONS += returns;
                        COMMISSIONS += returns;
                        
                    }
                   
               }
               
               if (!(inv->skipped_first_payment_day))
                    inv->skipped_first_payment_day = true;

               inv = inv->next;
           }
           
           pthread_mutex_unlock(&glock); 
            
        }

        acc_p = acc_p->next;
    }

    return true;
}

bool raise_rank(Account account, FILE *fout)
{
    /* if true; award onetime Director's recorgintion award...
       however, the award is in such a way that its descrete ie if one leaps
       from rank r to (r+4), they get the award for (r+4) not all the awards from
       (r+1) through (r+4)!
    */

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO raise-rank. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("raise-rank\n");

    bool rank_raised = false;
    if(!account) return rank_raised;
    if(account->is_consumer) return rank_raised;
    if(account->rank==11) return rank_raised; /* already at maximum rank */

    unsigned int rank = account->rank;

    pthread_mutex_lock(&glock);
    
    while((RANK_DETAILS[rank][1]!=1) && ++rank)
    {
        if (account->rank<=8)
        {
            if(!(
                (account->pv + account->leg_volumes[0] + account->leg_volumes[1] +
                account->leg_volumes[2]) >= RANK_DETAILS[rank][2]
            )) {--rank; break;}
        }
        else
        {
            if (!((account->highest_leg_ranks[0]>=RANK_DETAILS[rank][0])
                || (account->highest_leg_ranks[1]>=RANK_DETAILS[rank][0])
                || (account->highest_leg_ranks[2]>=RANK_DETAILS[rank][0])
            )){--rank; break;}
        }
    }

    rank==12 ? --rank : rank;

    pthread_mutex_unlock(&glock);

    if(rank==account->rank) return rank_raised;

    rank_raised = true;
    
    // the logic for the one-time rank-change lumpsum changed n we decided to leave this out of the first
    // released version hence the comment for this code
    /*
    
    if(RANK_DETAILS[rank][3])
    {
        unsigned int buff_length=0;
        String reason_strings[] = {"DRA, new rank is <", RANKS[rank], ">", "\0"};
        length_of_all_strings(reason_strings, &buff_length);
        char buff[buff_length+1];
        join_strings(buff,reason_strings);

        award_commission(account, RANK_DETAILS[rank][3], "DRA", buff, fout);
    }
    
    */
    
    pthread_mutex_lock(&glock);

    account->rank = rank;
    
    pthread_mutex_unlock(&glock);
    
    return rank_raised;
}

bool calculate_tvc(Account account, FILE *fout)
{
    /* this function should be called only once a month on the agreed day */

    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO calculate TVC. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("calculate TVC\n");

    time_t t; t = time(NULL);
    struct tm *today = localtime(&t);

    if(today->tm_mday!=PAYMENT_DAY){ghfu_warn(15,fout); return false;}

    Amount lower_leg_volume, actual_lower_leg_volume, returns;
    unsigned int i, j, buff_length; /* i is used for getting the lower_leg_volume, while j is used to
                              get the TVC % comission based on the account PV
                           */

    Commission new_commission;

    char returns_str[16], llv_str[32], allv_str[32];

    if(account)
    {
        pthread_mutex_lock(&glock);

        if(
            /* since logic operators are processed left to right, this code won't 
               break when account->children->next==NULL!
            */
            !(account->is_consumer) &&
            account->pv_made_in_first_month &&
           (account->children) && 
           (account->children->next) && 
           /* in addition to having atleast 2 direct-children, each child must have 
              atleast 1 direct child of their own */
           (((Account)(account->children->account))->children) &&
           (((Account)(account->children->next->account))->children)
           )
        {
            lower_leg_volume = account->leg_volumes[0];
            i = 0;
            for(; i<2/*only the first 2 legs considered*/; ++i)
                lower_leg_volume = (account->leg_volumes[i])<lower_leg_volume ? 
                    account->leg_volumes[i] : lower_leg_volume ;

            actual_lower_leg_volume = lower_leg_volume - account->TVC_levels[0];
            account->TVC_levels[0] = lower_leg_volume;
            
            j = 0;
            for(;TVC[j][1];++j)
            {
                if(account->pv < TVC[j+1][1]) break;
            }
            
            TVC[j][1] ? j : --j;
            
            returns = TVC[j][1]*.01*actual_lower_leg_volume;
            
            /* returns should not be more than PV made in the first month!!!! */
            returns = returns > account->pv_made_in_first_month ? 
                account->pv_made_in_first_month : returns;
            
            if(actual_lower_leg_volume) /* you dont wanna waste resources on 0-value commissions */
            {
                        new_commission = malloc(sizeof(struct commission));
                        if(!new_commission) memerror(fout);

                        new_commission->date = time(NULL);

                        new_commission->reason = NULL;
                        new_commission->amount = returns;

                        new_commission->next = NULL;
                        new_commission->prev = NULL;

                        if(!account->commissions)
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
                        new_commission->reason = malloc(sizeof(char)*(buff_length+1));
                        if(!(new_commission->reason)) {gfree(new_commission); memerror(fout);}
                        join_strings(new_commission->reason,reason_strings);

                        account->available_balance += returns;
                        account->total_returns += returns;
                        
                        CUMULATIVE_COMMISSIONS += returns;
                        COMMISSIONS += returns;
            
            }
        }
        
        /* else{fprintf(fout,"not TVC for <%s>...",account->names); ghfu_warn(14,fout);} */
        
        pthread_mutex_unlock(&glock);

        return true;
    }

    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = !acc_p ? acc_p : acc_p->next;

    while(acc_p)
    {
        pthread_mutex_lock(&glock);

        acc = acc_p->account;

        if(
            /* since logic operators are processed left to right, this code won't 
               break when account->children->next==NULL or when account->children->next->next==NULL!
            */
            !(acc->is_consumer) &&
            acc->pv_made_in_first_month &&
           (acc->children) && 
           (acc->children->next) && 
           /* in addition to having atleast 2 direct-children, each child must have 
              atleast 1 direct child of their own */
           (((Account)(acc->children->account))->children) &&
           (((Account)(acc->children->next->account))->children)
           )
        {
            lower_leg_volume = acc->leg_volumes[0];
            i = 0;
            for(; i<2/*only the first 2 legs considered*/; ++i)
                lower_leg_volume = (acc->leg_volumes[i])<lower_leg_volume ? 
                    acc->leg_volumes[i] : lower_leg_volume ;

            actual_lower_leg_volume = lower_leg_volume - acc->TVC_levels[0];
            acc->TVC_levels[0] = lower_leg_volume;
            
            j = 0;
            for(;TVC[j][1];++j)
            {
                if(acc->pv < TVC[j+1][1]) break;
            }
            
            TVC[j][1] ? j : --j;
            
            returns = TVC[j][1]*.01*actual_lower_leg_volume;

            /* returns should not be more than PV made in the first month!!!! */
            returns = returns > acc->pv_made_in_first_month ? 
                acc->pv_made_in_first_month : returns;
            
            if(actual_lower_leg_volume) /* you dont wanna waste resources on 0-value commissions */
            {
                        new_commission = malloc(sizeof(struct commission));
                        if(!new_commission) memerror(fout);

                        new_commission->date = time(NULL);

                        new_commission->reason = NULL;
                        new_commission->amount = returns;

                        new_commission->next = NULL;
                        new_commission->prev = NULL;

                        if(!(acc->commissions))
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
                        new_commission->reason = malloc(sizeof(char)*(buff_length+1));
                        if(!(new_commission->reason)) {gfree(new_commission); memerror(fout);}
                        join_strings(new_commission->reason,reason_strings);

                        acc->available_balance += returns;
                        acc->total_returns += returns;
                        
                        CUMULATIVE_COMMISSIONS += returns;
                        COMMISSIONS += returns;
            
            }
        }

        acc_p = acc_p->next;

        pthread_mutex_unlock(&glock);
    }

    return true;
}

bool award_rank_monthly_bonuses(Account account, FILE *fout)
{
    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO award rank-monthly bonuses. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("award rank_monthly bonuses\n");

    time_t t; t = time(NULL);
    struct tm *today = localtime(&t);

    if(today->tm_mday!=PAYMENT_DAY){ghfu_warn(20,fout); return false;}

    Amount lower_leg_volume, actual_lower_leg_volume;
    unsigned int i;

    if(account)
    {
        pthread_mutex_lock(&glock);

        if(!(account->is_consumer) && (account->rank>=6)) /* rank-bonuses for rank>=Senior Director*/
        {
            // fancy-pants bonuses (HOB,LCB,EAB) are given on the third leg
            lower_leg_volume = account->leg_volumes[2];
            /*
            i = 0;
            for(; i<3; ++i)
                lower_leg_volume = (account->leg_volumes[i])<lower_leg_volume ? 
                    account->leg_volumes[i] : lower_leg_volume ;

            actual_lower_leg_volume = lower_leg_volume - account->TVC_levels[1];
            account->TVC_levels[1] = lower_leg_volume;
            */

            pthread_mutex_unlock(&glock);                
            
            /* are these funny bonuses given on the actual_LLV or on the LLV? i used the LLV */
            if(lower_leg_volume)
            {
                if(account->rank<11)
                {
                    award_commission(account, lower_leg_volume, "HOB", "HOB",fout);
                                    
                    if(account->rank>=8)
                    {
                        award_commission(account, lower_leg_volume, "LCB", "LCB",fout);
                        award_commission(account, lower_leg_volume, "EAB", "EA bonus",fout);
                    }
                }
                else
                    award_commission(account, lower_leg_volume, "EAB", "EA bonus",fout);
            }
        }
        else
            pthread_mutex_unlock(&glock);                

        return true;
    }

    pthread_mutex_lock(&glock);

    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = !acc_p ? acc_p : acc_p->next;

    pthread_mutex_unlock(&glock);

    while(acc_p)
    {
        pthread_mutex_lock(&glock);

        acc = acc_p->account;

        if(!(acc->is_consumer) && acc->rank>=6) /* rank-bonuses for rank>=Senior Director*/
        {
            // fancy-pants bonuses (HOB,LCB,EAB) are given on the third leg
            lower_leg_volume = acc->leg_volumes[2];
            /*
            i = 0;
            for(; i<3; ++i)
                lower_leg_volume = (acc->leg_volumes[i])<lower_leg_volume ? 
                    acc->leg_volumes[i] : lower_leg_volume ;

            actual_lower_leg_volume = lower_leg_volume - acc->TVC_levels[1];
            acc->TVC_levels[1] = lower_leg_volume;
            */

            pthread_mutex_unlock(&glock);
            
            /* are these funny bonuses given on the actual_LLV or on the LLV? i used the LLV */
            if(lower_leg_volume)
            {
                if(acc->rank<11)
                {
                    award_commission(acc, lower_leg_volume, "HOB", "HOB",fout);
                                    
                    if(acc->rank>=8)
                    {
                        award_commission(acc, lower_leg_volume, "LCB", "LCB",fout);
                        award_commission(acc, lower_leg_volume, "EAB", "EA bonus",fout);
                    }
                }
                else
                    award_commission(acc, lower_leg_volume, "EAB", "EA bonus",fout);
            }
        }
        else
            pthread_mutex_unlock(&glock);                

        acc_p = acc_p->next;

    }

    return true;
}

void show_commissions(Account account)
{
    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    if(!GLOCK_INITIALISED)
    {
        fprintf(stdout, "FAILED TO show commissions. glock NOT INITIALISED. did you call init?"); 
        return;
    }

    //printf("show commissions\n");

    Commission c;
    struct tm *lt;
    
    if (account)
    {        
        printf("\n  %s's COMMISSIONS & BONUSES\n",account->names);

        pthread_mutex_lock(&glock);
        
        c = account->commissions;
        if(!c) printf("    None\n");

        int i = 1;
        while(c)
        {
            lt = localtime(&(c->date));
            printf("    %d) %d/%d/%d: %s\n",i, 
                lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900),
                c->reason); c=c->next; ++i;
        }        
        
        pthread_mutex_unlock(&glock);
        
        return;
    }

    pthread_mutex_lock(&glock);

    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = !acc_p ? acc_p : acc_p->next;

    pthread_mutex_unlock(&glock);


    while(acc_p)
    {
        
        acc = acc_p->account;

        printf("\n  %s's COMMISSIONS & BONUSES\n",acc->names);
        
        pthread_mutex_lock(&glock);
        
        c = acc->commissions;
        if(!c) printf("    None\n");

        int i = 1;
        while(c)
        {
            lt = localtime(&(c->date));
            printf("    %d) %d/%d/%d: %s\n",i, 
                lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900),
                c->reason); c=c->next; ++i;
        }

        pthread_mutex_unlock(&glock);

        acc_p = acc_p->next;
    }
}

bool dump_commissions(const Account account, FILE *fout)
{
    /* dump commissions to json file */

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO dump commissions. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("dump commissions\n");

    Commission c;
    struct tm *lt;
    
    if (account)
    {
        pthread_mutex_lock(&glock);

        c = account->commissions;
        fprintf(fout, ",\"commissions\": [");

        bool started=false;
        while(c)
        {
            if(started) fprintf(fout,",");
            started = started ? started : true;
            
            lt = localtime(&(c->date));

            fprintf(fout,"[\"%d/%d/%d\", %.2f,\"%s\"]",
                lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900),
                c->amount, c->reason);
            c=c->next;
            
        }
        
        fprintf(fout, "]");

        pthread_mutex_unlock(&glock);
    
    }

    return true;
}

void show_leg_volumes(Account account)
{
    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    if(!GLOCK_INITIALISED)
    {
        fprintf(stdout, "FAILED TO show leg volumes. glock NOT INITIALISED. did you call init?"); 
        return;
    }

    //printf("show LVs\n");

    if(account)
    {
        pthread_mutex_lock(&glock);

        printf("%s leg volumes: [%.0f, %.0f, %.0f]\n",account->names, 
            account->leg_volumes[0], account->leg_volumes[1], account->leg_volumes[2]);

        pthread_mutex_unlock(&glock);

        return;
    }

    pthread_mutex_lock(&glock);

    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = !acc_p ? acc_p : acc_p->next;

    pthread_mutex_unlock(&glock);


    while(acc_p)
    {
        acc = acc_p->account;

        pthread_mutex_lock(&glock);

        printf("%s leg volumes: [%.0f, %.0f, %.0f]\n",acc->names, 
            acc->leg_volumes[0], acc->leg_volumes[1], acc->leg_volumes[2]);

        acc_p = acc_p->next;

        pthread_mutex_unlock(&glock);
    }
}

bool dump_leg_volumes(const Account account, FILE *fout)
{
    /* dump leg volumes to json file */

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO dump-leg-volumes. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("dump LV's\n");

    if(account)
    {
        pthread_mutex_lock(&glock);
        
        fprintf(fout, ",\"leg_volumes\":[%.2f,%.2f,%.2f]",
            account->leg_volumes[0], account->leg_volumes[1], account->leg_volumes[2]);
        
        pthread_mutex_unlock(&glock);
    }
    
    return true;
}

void show_investments(const Account account)
{
    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    if(!GLOCK_INITIALISED)
    {
        fprintf(stdout, "FAILED TO show investments. glock NOT INITIALISED. did you call init?"); 
        return;
    }

    //printf("show investments\n");

    Investment inv;
    struct tm *lt;
    if(account)
    {
        printf("\n  %s's INVESTMENTS\n",account->names);

        pthread_mutex_lock(&glock);

        inv = account->investments;

        if(!inv) printf("    None\n");
        
        while(inv)
        {
            lt = localtime(&(inv->date));
            printf("    Date (DD/MM/YYY): %d:%d, %d/%d/%d\n",
                lt->tm_hour,lt->tm_min,lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900));
            printf("    Weight: %.2f points\n",inv->amount);
            printf("    Package: %s\n",inv->package);
            printf("    Months returned: %d\n",inv->months_returned);
            printf("    Total investment returns: %.2f\n",inv->returns);
            printf("    Total returns at creation: %.2f\n",inv->total_returns_on_creation);
            
            inv = inv->next;
            
            if(inv) printf("    --------\n");
        }

        pthread_mutex_unlock(&glock);

        return;
    }
    
    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = !acc_p ? acc_p : acc_p->next;
    
    while(acc_p)
    {
        acc = acc_p->account;

        printf("\n  %s's INVESTMENTS\n",acc->names);

        pthread_mutex_lock(&glock);

        inv = acc->investments;

        if(!inv) printf("    None\n");

        while(inv)
        {
            lt = localtime(&(inv->date));
            printf("    Date (DD/MM/YYY): %d:%d, %d/%d/%d\n",
                lt->tm_hour,lt->tm_min,lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900));
            printf("    Weight: %.2f points\n",inv->amount);
            printf("    Package: %s\n",inv->package);
            printf("    Months returned: %d\n",inv->months_returned);
            
            inv = inv->next;
            
            if(inv) printf("    --------\n");
        }

        acc_p = acc_p->next;

        pthread_mutex_unlock(&glock);
    }
}


bool dump_investments(const Account account, FILE *fout)
{
    /* dump investments to json file */

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO dump investments. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("dump investments\n");

    Investment inv;
    struct tm *lt;
    if(account)
    {
        pthread_mutex_lock(&glock);
        
        inv = account->investments;

        fprintf(fout, ",\"investments\":[");
        
        bool started = false;
        while(inv)
        {
            if(started) fprintf(fout, ",");
            started = started ? started : true;

            lt = localtime(&(inv->date));

            fprintf(fout, "[\"%d:%d, %d/%d/%d\",%.2f,\"%s\",%ld,%d, %.2f, %.2f]",
                lt->tm_hour,lt->tm_min,lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900),
                inv->amount,inv->package,inv->package_id,inv->months_returned, inv->returns,
                inv->total_returns_on_creation);
            inv = inv->next;            
        }

        fprintf(fout, "]");

        pthread_mutex_unlock(&glock);

    }

    return true;
}

void show_withdraws(const Account account)
{
    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    if(!GLOCK_INITIALISED)
    {
        fprintf(stdout, "FAILED TO show withdraws. glock NOT INITIALISED. did you call init?"); 
        return;
    }

    //printf("show investments\n");

    Withdraw w;
    struct tm *lt;
    int i;
    if(account)
    {
        printf("\n  %s's WITHDRAWS\n",account->names);

        pthread_mutex_lock(&glock);

        w = account->withdraws;

        if(!w) printf("    None\n");
        
        i = 1;
        
        while(w)
        {
            lt = localtime(&(w->date));
            printf("    %d) %d:%d, %d/%d/%d: $%.2f\n",
                i, lt->tm_hour,lt->tm_min,lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900), w->amount);
            
            w = w->next;            
        }

        pthread_mutex_unlock(&glock);

        return;
    }
    
    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = !acc_p ? acc_p : acc_p->next;
    
    while(acc_p)
    {

        pthread_mutex_lock(&glock);
        acc = acc_p->account;

        printf("\n  %s's WITHDRAWS\n",acc->names);

        pthread_mutex_lock(&glock);

        w = acc->withdraws;

        if(!w) printf("    None\n");
        
        i = 1;
        
        while(w)
        {
            lt = localtime(&(w->date));
            printf("    %d) %d:%d, %d/%d/%d: $%.2f\n",
                i, lt->tm_hour,lt->tm_min,lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900), w->amount);
            
            w = w->next;            
        }

        acc_p = acc_p->next;

        pthread_mutex_unlock(&glock);
    }
}


bool dump_withdraws(const Account account, FILE *fout)
{
    /* dump investments to json file */

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO dump withdraws. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("dump investments\n");

    Withdraw w;
    struct tm *lt;
    if(account)
    {
        pthread_mutex_lock(&glock);
        
        w = account->withdraws;

        fprintf(fout, ",\"withdraws\":[");
        
        bool started = false;
        while(w)
        {
            if(started) fprintf(fout, ",");
            started = started ? started : true;

            lt = localtime(&(w->date));

            fprintf(fout, "[\"%d:%d, %d/%d/%d\",%.2f]",
                lt->tm_hour,lt->tm_min,lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900),
                w->amount);
            w = w->next;            
        }

        fprintf(fout, "]");

        pthread_mutex_unlock(&glock);

    }

    return true;
}


void show_direct_children(const Account account)
{
    if(!GLOCK_INITIALISED)
    {
        fprintf(stdout, "FAILED TO show direct children. glock NOT INITIALISED. did you call init?"); 
        return;
    }

    //printf("show direct children\n");

    AccountPointer acc_p, acc_p_child;
    Account acc, acc_child;

    if(account)
    {
        printf("\n  %s's DIRECT DESCENDANTS\n",account->names);

        pthread_mutex_lock(&glock);

        acc_p = account->children;
        
        if(!acc_p) printf("    None\n");

        int i=1;
    
        while(acc_p)
        {
            acc = acc_p->account;
            printf("    %d) %s\n",i, acc->names); 

            acc_p=acc_p->next; 
            ++i;
        }

        pthread_mutex_unlock(&glock);

        return;
    }
    
    acc_p = HEAD;
    acc_p = !acc_p ? acc_p : acc_p->next;

    while(acc_p)
    {
        pthread_mutex_lock(&glock);

        acc = acc_p->account;
        printf("\n  %s's DIRECT DESCENDANTS\n",acc->names);

        acc_p_child = acc->children;
        
        if(!acc_p_child) printf("    None\n");

        int i=1;
    
        while(acc_p_child)
        {
            acc_child = acc_p_child->account;
            printf("    %d) %s\n",i, acc_child->names); 

            acc_p_child=acc_p_child->next; 
            ++i;
        }

        acc_p = acc_p->next;

        pthread_mutex_unlock(&glock);

    }
}

bool dump_direct_children(const Account account, FILE *fout)
{
    /* dump account direct children to json file */

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO dump direct children. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("dump direct children\n");

    AccountPointer acc_p;
    Account acc;

    if(account)
    {
        pthread_mutex_lock(&glock);
        
        acc_p = account->children;
        
        fprintf(fout, ",\"direct_children\":[");
        

        bool started = false;
        while(acc_p)
        {
            if(started) fprintf(fout, ",");
            started = started ? started : true;
            acc = acc_p->account;
            fprintf(fout,"\"%s\"",acc->names); 

            acc_p=acc_p->next; 
        }

        fprintf(fout, "]");

        pthread_mutex_unlock(&glock);

    }

    return true;
}

void structure_details(const Account account)
{
    /* the code for account==NULL is the same as that for otherrwise but in a loop. i dint want to 
       make the function recursive as this becomes CPU expensive in the long run */

    /* atempt to provide all details about all members in the system */

    if(!GLOCK_INITIALISED)
    {
        fprintf(stdout, "FAILED TO show structure details. glock NOT INITIALISED. did you call init?"); 
        return;
    }

    struct tm *lt;

    //printf("show structure details\n");

    if(account)
    {

        pthread_mutex_lock(&glock);

        lt = localtime(&(account->date));

        printf("\n%s\n",account->names);
        printf("  Account creation date: %d/%d/%d\n", lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900));
        printf("  ID: %ld\n",account->id);
        printf("  Uplink: %s\n", !(account->uplink) ? "ROOT" : account->uplink->names);
        printf("  PV made in first month = %.2lf points\n",account->pv_made_in_first_month);
        printf("  CPV = %.2lf points\n",account->pv);
        printf("  Total Returns = $%.2lf\n",account->total_returns);
        printf("  Available Balance = $%.2lf\n",account->available_balance);
        printf("  Total Redeemed = $%.2lf\n",account->total_redeems);
        printf("  Leg Volumes = (%.2lf, %.2lf, %.2lf)\n",
            account->leg_volumes[0],account->leg_volumes[1],account->leg_volumes[2]);
        printf("  Cummulative Organisation Volume: %.f points\n",
            account->pv+account->leg_volumes[0]+account->leg_volumes[1]+account->leg_volumes[2]
        );
        printf("  Rank: %s\n",RANKS[account->rank]);
        
        pthread_mutex_unlock(&glock);
        
        show_investments(account);
        show_commissions(account);
        show_direct_children(account);
        show_withdraws(account);
        
        return;
    }
    
    AccountPointer acc_p = HEAD;
    Account acc;
    acc_p = !acc_p ? acc_p : acc_p->next;
    
    while(acc_p)
    {

        pthread_mutex_lock(&glock);

        acc = acc_p->account;

        lt = localtime(&(acc->date));

        printf("\n%s\n",acc->names);
        printf("  Account creation date: %d/%d/%d\n", lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900));
        printf("  ID: %ld\n",acc->id);
        printf("  Uplink: %s\n", !(acc->uplink) ? "ROOT" : acc->uplink->names);
        printf("  PV made in first month = %.2lf points\n",acc->pv_made_in_first_month);
        printf("  CPV = %.2lf points\n",acc->pv);
        printf("  Total Returns = $%.2lf\n",acc->total_returns);
        printf("  Available Balance = $%.2lf\n",acc->available_balance);
        printf("  Total Redeemed = $%.2lf\n",acc->total_redeems);
        printf("  Leg Volumes = (%.2lf, %.2lf, %.2lf)\n",
            acc->leg_volumes[0],acc->leg_volumes[1],acc->leg_volumes[2]);
        printf("  Cummulative Organisation Volume: %.f points\n",
            acc->pv+acc->leg_volumes[0]+acc->leg_volumes[1]+acc->leg_volumes[2]
        );

        printf("  Rank: %s\n",RANKS[acc->rank]);
        
        pthread_mutex_unlock(&glock);

        show_investments(acc);
        show_commissions(acc);
        show_direct_children(acc);
        show_withdraws(acc);

        acc_p = acc_p->next;
    }
    
}

bool dump_structure_details(ID account_id, String fname)
{
    /* this function creates a json file describing the account in all its detail. the json is then sent
       to the web API requesting for the account information
       fname is a full path to the output json file
    */

    FILE *fout = fopen(fname, "w");
    if(!fout) return false;

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO dump_structure. glock NOT INITIALISED. did you call init?");
        fclose(fout);
        return false;
    }

    //printf("dump structure details...");

    Account account = get_account_by_id(account_id);
    if(!account) {ghfu_warn(28,fout); fclose(fout); return false;}

    //printf("done\n");

    bool status = false;
    struct tm *lt;

    lt = localtime(&(account->date));

    fprintf(fout, "{");

    pthread_mutex_lock(&glock);

    /* personal info */
    if (!(account->is_consumer))
    {
        fprintf(fout, "\"names\":\"%s\",\"id\":%ld,\"uplink\":\"%s\","
            "\"pv\":%.2f,\"total_returns\":%.2f,\"available_balance\":%.2f,\"total_redeems\":%.2f,"
            "\"rank\":\"%s\",\"highest-leg-ranks\":[\"%s\",\"%s\",\"%s\"], \"COV\":%.2f"
            ", \"creation-date\":\"%d/%d/%d\", \"first-month-pv\":%.2f"
            ",\"number_of_withdraws\":%d",
            account->names, account->id, (!(account->uplink) ? "ROOT" : account->uplink->names),
            account->pv,account->total_returns,account->available_balance,account->total_redeems,
            RANKS[account->rank], RANKS[account->highest_leg_ranks[0]], RANKS[account->highest_leg_ranks[1]],
            RANKS[account->highest_leg_ranks[2]],
            account->pv+account->leg_volumes[0]+account->leg_volumes[1]+account->leg_volumes[2],
            lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900),account->pv_made_in_first_month,
            account->redeems
        );
    }
    else
    {
        fprintf(fout, "\"names\":\"%s\",\"id\":%ld,\"uplink\":\"%s\","
            "\"pv\":%.2f,\"total_returns\":%.2f,\"available_balance\":%.2f,\"total_redeems\":%.2f,"
            "\"creation-date\":\"%d/%d/%d\""
            ",\"number_of_withdraws\":%d",
            account->names, account->id, (!(account->uplink) ? "ROOT" : account->uplink->names),
            account->pv,account->total_returns,account->available_balance,account->total_redeems,            
            lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900),
            account->redeems
        );    
    }
    pthread_mutex_unlock(&glock);

    /* the other info */
    status = true;
    if (!(account->is_consumer))
    {
        dump_leg_volumes(account,fout) ? 1: (status=false);
        dump_investments(account, fout) ? 1: (status=false);
        dump_direct_children(account, fout) ? 1: (status=false);
    }
    dump_commissions(account, fout) ? 1: (status=false);
    dump_withdraws(account, fout) ? 1: (status=false);

    fprintf(fout, "}");

    fclose(fout);


    /* now encrypt generated db_file(is it necessary? i doubt bcoz python/any other adaptor will delete
       the json file as soon as it reads from it) */

    return status;
}

bool redeem_points(Account account, Amount amount, bool test_feasibility, FILE *fout)
{
    /* if this function returns true but the money actually is'nt redeemed for some reason (loss in
       network for example or when the mobile money API is down), please REVERSE this operation or the 
       member will actually lose their returns permanently! 
    */

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO redeem points. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("redeem-points\n");

    if(!account) {ghfu_warn(11,fout); return false;}
    //if(account->id==1) {ghfu_warn(24,fout); return false;} // system accounts cant redeem
    if(amount>(account->available_balance)){ghfu_warn(10,fout); return false;}
    if(amount<0) {ghfu_warn(36,fout); return false;}

    if (test_feasibility) return true; /* no more checks, operation is feasible*/

    pthread_mutex_lock(&glock);

    /* create new withdraw and add it to the account's withdraws */
    Withdraw new_withdraw = malloc(sizeof(struct withdraw));
    if(!new_withdraw) memerror(fout);

    new_withdraw->date = time(NULL);
    new_withdraw->amount = amount;

    new_withdraw->next = NULL;
    new_withdraw->prev = NULL;

    if(!(account->withdraws))
        account->withdraws = new_withdraw;
    else
    {
        new_withdraw->prev = account->last_withdraw;
        account->last_withdraw->next = new_withdraw;
    }
    
    account->last_withdraw = new_withdraw;

    
    account->available_balance -= amount;
    account->total_redeems += amount;
    ++(account->redeems);
    
    COMMISSIONS -= amount;

    /*
        the actual system floats n expenses aint modified...sohuld they?
    */

    pthread_mutex_unlock(&glock);

    return true;
}

bool redeem_account_points(ID account_id, Amount amount, bool test_feasibility, String fout_name)
{
    /* python/java/etc interface to redeem_points */

    FILE *fout = fopen(fout_name, "w");
    if(!fout) return false;

    Account account = get_account_by_id(account_id);

    bool redeemed = redeem_points(account, amount, test_feasibility, fout);

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
    
        the function has O(N/2) complexity because we have a tail(TAIL). since we are searching for accounts,
        this must suffice(100 million accounts will appear to be 50 million accounts when searching for target)
        but if there is a need to make this even faster, we can introduce the concept of <middle> such that
        there are three main account access pointers; HEAD,MIDDLE,TAIL. this gives us a new O(N/4) complexity.
        implementation logic?
            1) in the beginning, all (H,M&T) are NULL
            2) when first account is created, all (H,M&T) point to the first account
            3) as new accounts are added, H is stationary, T points to the new added account and M changes as;
                - if new ID is even, dont change M
                - otherwise, M = M->next
                
                this ensures that M is so easy to update whenever a new account is created
    
    */

    if(!GLOCK_INITIALISED)
    {
        fprintf(stdout, "FAILED TO get_account_by_id. glock NOT INITIALISED. did you call init?"); 
        return NULL;
    }

    //printf("get account by id\n");


    pthread_mutex_lock(&glock);
    if((id>CURRENT_ID) || (!HEAD) || !(HEAD->next)) 
        {pthread_mutex_unlock(&glock); return NULL;}

    /* search from HEAD or from TAIL depending on if the id is closer to the last member id or not
       the problem with this however is that since the ID-counter never decrements(even when amember
       is deleted from the system), the id may be closer to the HEAD than the TAIL when a sufficiently
       large group of members have been deleted but the calculation will assume the id is still closer
       to the TAIL as only the TAIL id is considered!
    */
    AccountPointer acc_p = (TAIL==HEAD) ? HEAD->next :
        (id > (((Account)(TAIL->account))->id)/2 ? TAIL : HEAD->next);
    
    
    bool ascending = acc_p==HEAD->next ? true : false;
    pthread_mutex_unlock(&glock);
    
    while(acc_p)
    {
        pthread_mutex_lock(&glock);
        
        if(((Account)(acc_p->account))->id==id) {pthread_mutex_unlock(&glock); break;}

        /* break flow the moment its detected that the account was deleted. this is made easy by 
           the fact that id's are incremental */
        if(ascending && (((Account)(acc_p->account))->id > id)) 
            {pthread_mutex_unlock(&glock); return NULL;}
        if((!ascending) && (((Account)(acc_p->account))->id < id))
            {pthread_mutex_unlock(&glock); return NULL;}

        acc_p = ascending ? acc_p->next: acc_p->prev;
    
        pthread_mutex_unlock(&glock);
    }
    
    pthread_mutex_lock(&glock);
    
    Account acc_match = !acc_p ? NULL : (Account)(acc_p->account); /*if return is NULL, that account was deleted*/
    
    pthread_mutex_unlock(&glock);
    
    return acc_match;
}

ID account_id(Account account)
{
    if(!account) return 0;
    return account->id;
}

bool monthly_operations(FILE *fout)
{

    /* perform all monthly operations */
    
    bool status = true;

    auto_refill(NULL, fout) ? true: (status=false);
    calculate_tvc(NULL,fout) ? true: (status=false);
    
    award_rank_monthly_bonuses(NULL, fout) ? true: (status=false);

    return status;
}

bool perform_monthly_operations(String fout_name)
{
    /* python/java/etc interface to monthly_operations */

    bool status = false;

    FILE *fout = fopen(fout_name, "w"); 
    if(!fout) return false;

    fclose(fout);

    fout = fopen(fout_name, "a");
    if(!fout) return false;
    
    if(!MONTHLY_AUTO_REFILL_PERCENTAGES)
    {
        fprintf(fout, "no percentages given and the defaults are still not set!\n");
        fclose(fout);
        return status;
    }

    status = monthly_operations(fout);
    
    fclose(fout);

    return status;
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
    if(!GLOCK_INITIALISED)
        return false;

    //printf("set constant\n");

    if (value<=0) return false; 
    
    bool status = true;
    
    pthread_mutex_lock(&glock);

    if(!strcmp(constant, "payment-day")) PAYMENT_DAY = (int)value;
    else if(!strcmp(constant, "point-factor")) POINT_FACTOR = value;
    else if(!strcmp(constant, "account-creation-fee")) ACCOUNT_CREATION_FEE = value;
    else if(!strcmp(constant, "annual-subscription-fee")) ANNUAL_SUBSCRIPTION_FEE = value;
    else if(!strcmp(constant, "operations-fee")) OPERATIONS_FEE = value;
    else if(!strcmp(constant, "minimum-investment")) MINIMUM_INVESTMENT = value;
    else if(!strcmp(constant, "maximum-investment")) MAXIMUM_INVESTMENT = value;
    else if(!strcmp(constant, "last-investment-day")) LAST_INVESTMENT_DAY = (int)value;    
    else if(!strcmp(constant, "exchange-rate")) EXCHANGE_RATE = (int)value;
    else if(!strcmp(constant, "withdraw-charge")) WITHDRAW_CHARGE = value;
    else if(!strcmp(constant, "rate-inflate")) RATE_INFLATE = (int)value;
    else if(!strcmp(constant, "available-investments")) AVAILABLE_INVESTMENTS = (ID)value;

    else status = false;

    /* dump changes to file so that init sets them the next time the system is rebooted */

    pthread_mutex_unlock(&glock);

    return status;
}

bool dump_constants(String jermCrypt_path, String save_dir)
{

    if(!GLOCK_INITIALISED)
    {
        fprintf(stdout, "FAILED TO dump constants. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("dump constants\n");

    bool status = false;

    pthread_mutex_lock(&glock);

    unsigned int buff_length;
    String crypt_lib_paths[] = {jermCrypt_path,"/",JERM_CRYPT_LIB,"\0"};
    String data_file_paths[] = {save_dir, "/",DATA_FILE,"\0"};
    String jermCryptReply;
    
    length_of_all_strings(crypt_lib_paths, &buff_length);
    char crypt_lib_path[buff_length+1];
    join_strings(crypt_lib_path, crypt_lib_paths);

    length_of_all_strings(data_file_paths, &buff_length);
    char data_file_path[buff_length+1];
    join_strings(data_file_path, data_file_paths);

    FILE *fout = fopen(data_file_path,"wb");

    if(!fout) {pthread_mutex_unlock(&glock); return status;}

    /* load function from jermCrypt library eg; */
    void *libjermCrypt = dlopen(crypt_lib_path, RTLD_GLOBAL|RTLD_LAZY);

    if(!libjermCrypt) {fclose(fout); pthread_mutex_unlock(&glock); return status;}

    /* encrypt_file prototype in libjermCrypt.so */
    // char *encrypt_file(char *fin, char *pswd, char *fout, int overwrite, int verbose)
    
    char *(*encrypt_file)(char*,char*,char*,int,int) = dlsym(libjermCrypt, "encrypt_file");

    if(!encrypt_file) {fclose(fout); pthread_mutex_unlock(&glock); return status;}

    fprintf(fout, 
        "%d\x1%f\x1%f\x1%f\x1%f\x1%f\x1%f\x1%lf\x1%lf\x1%lf\x1"
        "%ld\x1%ld\x1%d\x1%d\x1%f\x1%d\x1%ld\x1%ld\x1", 
        PAYMENT_DAY,POINT_FACTOR,ACCOUNT_CREATION_FEE,ANNUAL_SUBSCRIPTION_FEE,OPERATIONS_FEE,
        MINIMUM_INVESTMENT,MAXIMUM_INVESTMENT,SYSTEM_FLOAT,CUMULATIVE_COMMISSIONS,COMMISSIONS,
        ACTIVE_ACCOUNTS,CURRENT_ID,LAST_INVESTMENT_DAY,EXCHANGE_RATE,WITHDRAW_CHARGE,RATE_INFLATE,
        AVAILABLE_INVESTMENTS,CURRENT_SERVICE_ID
        );

    /* dump current auto-refill percentages */

    if(MONTHLY_AUTO_REFILL_PERCENTAGES)
    {
        unsigned int num_of_arps=1;
        for(; MONTHLY_AUTO_REFILL_PERCENTAGES[num_of_arps-1][0]; ++num_of_arps);
        fprintf(fout, "%d\x1", num_of_arps);
        for(unsigned int i=0; i<(num_of_arps); ++i) /*dont dump the last {0,0} coz we are certain its always there! */
            fprintf(fout, "%.2f\x1%.2f\x1", 
                MONTHLY_AUTO_REFILL_PERCENTAGES[i][0],MONTHLY_AUTO_REFILL_PERCENTAGES[i][1]);
    }
    else
        fprintf(fout, "%d\x1", 0);

    fclose(fout);
    
    // encrypt file after writting data
    
    if(strcmp(
    jermCryptReply=encrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true, false), "Ok"))
    {
        printf("<dump_constants> encrypting file: %s\n",jermCryptReply);
        dlclose(libjermCrypt);
        pthread_mutex_unlock(&glock);
        return status;
    }        
    
    
    status = true;

    dlclose(libjermCrypt);

    pthread_mutex_unlock(&glock);
    
    return status;
}

bool load_constants(String jermCrypt_path, String save_dir)
{
    if(!GLOCK_INITIALISED)
    {
        fprintf(stdout, "FAILED TO load constants. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("load constants\n");


    bool status = false;

    unsigned int buff_length;

    pthread_mutex_lock(&glock);

    String crypt_lib_paths[] = {jermCrypt_path,"/",JERM_CRYPT_LIB,"\0"};
    String data_file_paths[] = {save_dir, "/",DATA_FILE,"\0"};
    String jermCryptReply;
    
    length_of_all_strings(crypt_lib_paths, &buff_length);
    char crypt_lib_path[buff_length+1];
    join_strings(crypt_lib_path, crypt_lib_paths);

    length_of_all_strings(data_file_paths, &buff_length);
    char data_file_path[buff_length+1];
    join_strings(data_file_path, data_file_paths);

    FILE *fin = fopen(data_file_path,"rb");

    if(!fin) {pthread_mutex_unlock(&glock); return status;}

    /* load function from jermCrypt library eg; */
    void *libjermCrypt = dlopen(crypt_lib_path, RTLD_GLOBAL|RTLD_LAZY);

    if(!libjermCrypt) {fclose(fin); pthread_mutex_unlock(&glock); return status;}

    /* encrypt_file & decrypt_file prototypes in libjermCrypt.so */
    // char *encrypt_file(char *fin, char *pswd, char *fout, int overwrite, int verbose);
    // char *decrypt_file(char *fin, char *pswd, char *fout, int overwrite, int verbose);

    char *(*encrypt_file)(char*,char*,char*,int,int) = dlsym(libjermCrypt, "encrypt_file");

    char *(*decrypt_file)(char*,char*,char*,int,int) = dlsym(libjermCrypt, "decrypt_file");

    if((!encrypt_file) || (!decrypt_file)) {fclose(fin); pthread_mutex_unlock(&glock); return status;}

    fclose(fin);

    // decrypt_data_file before reading from it...
    
    if(strcmp(
    jermCryptReply=decrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true,false), "Ok"))
    {
        printf("<load_constants> decrypting file to read data: %s\n",jermCryptReply);
        dlclose(libjermCrypt);
        pthread_mutex_unlock(&glock);

        DATA_FILE_PRESENT = true;
        
        return status;
    }
    

    fin = fopen(data_file_path,"rb");

    fscanf(fin, 
        "%d\x1%f\x1%f\x1%f\x1%f\x1%f\x1%f\x1%lf\x1%lf\x1%lf\x1"
        "%ld\x1%ld\x1%d\x1%d\x1%f\x1%d\x1%ld\x1%ld\x1", 
        &PAYMENT_DAY,&POINT_FACTOR,&ACCOUNT_CREATION_FEE,&ANNUAL_SUBSCRIPTION_FEE,&OPERATIONS_FEE,
        &MINIMUM_INVESTMENT,&MAXIMUM_INVESTMENT,&SYSTEM_FLOAT,&CUMULATIVE_COMMISSIONS,&COMMISSIONS,
        &ACTIVE_ACCOUNTS,&CURRENT_ID,&LAST_INVESTMENT_DAY,&EXCHANGE_RATE,&WITHDRAW_CHARGE,
        &RATE_INFLATE,&AVAILABLE_INVESTMENTS,&CURRENT_SERVICE_ID
        );

    /* extract monthly-auto-refill-percentages */
    unsigned int num_of_arps=0;
    fscanf(fin, "%d\x1", &num_of_arps);
    
    if(num_of_arps)
    {
        if(MONTHLY_AUTO_REFILL_PERCENTAGES)
        {
            bool deleting = true;
            for(int i=0; deleting; ++i)
            {
                if(!MONTHLY_AUTO_REFILL_PERCENTAGES[i][0]) deleting = false;
                gfree(MONTHLY_AUTO_REFILL_PERCENTAGES[i]);
            }
            gfree(MONTHLY_AUTO_REFILL_PERCENTAGES);
        }

        MONTHLY_AUTO_REFILL_PERCENTAGES = malloc(num_of_arps*sizeof(float*));
        if (!MONTHLY_AUTO_REFILL_PERCENTAGES) memerror(stdout);
        for(unsigned int i=0; i<num_of_arps; ++i)
        {
            MONTHLY_AUTO_REFILL_PERCENTAGES[i] = malloc(sizeof(float)*2);
            if(!MONTHLY_AUTO_REFILL_PERCENTAGES[i]) memerror(stdout);
            fscanf(fin, "%f\x1%f\x1", 
                MONTHLY_AUTO_REFILL_PERCENTAGES[i], 
                &(MONTHLY_AUTO_REFILL_PERCENTAGES[i][1]));
        }
    
    }
    

    fclose(fin);
    
    // encrypt data file when reading from it is done
    
    if(strcmp(
    jermCryptReply=encrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true,false), "Ok"))
    {
        printf("<load_constants> encrypting file after reading data: %s\n",jermCryptReply);
        dlclose(libjermCrypt);
        pthread_mutex_unlock(&glock);
        return status;
    }
    
            
    status = true;
    dlclose(libjermCrypt);

    pthread_mutex_unlock(&glock);
    
    return status;
}

bool save_structure(String jermCrypt_path, String save_dir)
{
    if(!GLOCK_INITIALISED)
    {
        fprintf(stdout, "FAILED TO save structure. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("save structure\n");

    if(!HEAD) return true; /* thereis nothing to save, so no error !*/
    
    bool status = false;

    unsigned int buff_length;

    pthread_mutex_lock(&glock);

    String crypt_lib_paths[] = {jermCrypt_path,"/",JERM_CRYPT_LIB,"\0"};
    String data_file_paths[] = {save_dir, "/",STRUCTURE_FILE,"\0"};
    String jermCryptReply;
    
    length_of_all_strings(crypt_lib_paths, &buff_length);
    char crypt_lib_path[buff_length+1];
    join_strings(crypt_lib_path, crypt_lib_paths);

    length_of_all_strings(data_file_paths, &buff_length);
    char data_file_path[buff_length+1];
    join_strings(data_file_path, data_file_paths);

    FILE *fout = fopen(data_file_path,"wb");

    if(!fout) {pthread_mutex_unlock(&glock); return status;}

    /* load function from jermCrypt library eg; */
    void *libjermCrypt = dlopen(crypt_lib_path, RTLD_GLOBAL|RTLD_LAZY);

    if(!libjermCrypt) {fclose(fout); pthread_mutex_unlock(&glock); return status;}

    /* encrypt_file prototype in libjermCrypt.so */
    // char *encrypt_file(char *fin, char *pswd, char *fout, int overwrite, int verbose)
    
    char *(*encrypt_file)(char*,char*,char*,int,int) = dlsym(libjermCrypt, "encrypt_file");

    if(!encrypt_file) 
        {dlclose(libjermCrypt); fclose(fout); pthread_mutex_unlock(&glock); return status;return status;}

    /* dump entire structure to the file now... */

    AccountPointer acc_p = HEAD->next, child;
    Account acc;

    Commission commission;
    Investment investment;
    Withdraw withdraw;
    Service service;
    Payment payment;

    unsigned long number_of_items;

    while(acc_p)
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
            6) dump the number of withdraws and then for each withdraw, dump the amount and date
               
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
                withdraws: [(136297584,23.89), (136297576,13.01)]
             
                the output would be(neglect the newlines);
                
                5\x156.23\x1300.89\x123.56\x10.00\x13\x156.23\x1124.98\x1489.26\x1869\x1248.79\x10
                \x11\x11\x10\x1
                12\x1glayn bukman
                3\x14\x19\x1
                2\x123.56\x18\x1reason 156.18\x117\x1wow this is cool!
                1\x113625468\x1200.03\x1163.89\x13\x1
                23\x15\x1pkg 1
                2\x1136297584\x123.89\x1136297576\x113.01\x1
             
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
            "%ld\x1%d\x1%.2f\x1%.2f\x1%.2f\x1%.2f\x1%ld\x1%.2f\x1%.2f\x1%.2f\x1%.2f\x1%.2f\x1%d\x1"
            "%d\x1%d\x1%d\x1%ld\x1%.2f\x1%d\x1%ld\x1%s",
            acc->id,acc->is_consumer,acc->pv,acc->available_balance,acc->total_returns,acc->total_redeems,
            (!(acc->uplink) ? 0 : acc->uplink->id), acc->leg_volumes[0], acc->leg_volumes[1],
            acc->leg_volumes[2],acc->TVC_levels[0],acc->TVC_levels[1], acc->rank,
            acc->highest_leg_ranks[0],acc->highest_leg_ranks[1],acc->highest_leg_ranks[2],
            acc->date, acc->pv_made_in_first_month,acc->redeems,
            strlen(acc->names),acc->names
        );

        /* start next section (children)*/

        child = acc->children;
        number_of_items = 0;
        if(!child) 
            fprintf(fout,"%ld\x1",number_of_items); 
        else
        {
            while(child) {++number_of_items; child=child->next;}
            fprintf(fout,"%ld\x1",number_of_items); 

            child = acc->children;

            while(child)
            {
                fprintf(fout, "%ld\x1",((Account)(child->account))->id);
                child = child->next;
            }
        }

        /* start next section (commissions)*/
        commission = acc->commissions;
        number_of_items = 0;
        if(!commission)
            fprintf(fout, "%ld\x1",number_of_items);
        else
        {
            while(commission){++number_of_items; commission=commission->next;}
            fprintf(fout, "%ld\x1",number_of_items);

            commission = acc->commissions;

            while(commission)
            {
                fprintf(fout, "%ld\x1%.2f\x1%ld\x1%s",
                    commission->date,
                    commission->amount, 
                    strlen(commission->reason), 
                    commission->reason);
                commission = commission->next;
            }
        }

        /* start next section (investments)*/
        number_of_items = 0;
        investment = acc->investments;

        if(!investment)
            fprintf(fout, "%ld\x1",number_of_items);
        else
        {
            while(investment){++number_of_items; investment=investment->next;}
            fprintf(fout, "%ld\x1",number_of_items);

            investment = acc->investments;
            while(investment)
            {
                fprintf(fout, "%ld\x1%.2f\x1%.2f\x1%d\x1%ld\x1%.2f\x1%d\x1%ld\x1%s",
                    investment->date, investment->amount,
                    investment->total_returns_on_creation,investment->skipped_first_payment_day,
                    investment->package_id, investment->returns, investment->months_returned,
                    strlen(investment->package), investment->package);
                investment = investment->next;
            }
        }

        /* start next section (withdraws)*/
        withdraw = acc->withdraws;
        number_of_items = 0;
        if(!withdraw)
            fprintf(fout, "%ld\x1",number_of_items);
        else
        {
            while(withdraw){++number_of_items; withdraw=withdraw->next;}
            fprintf(fout, "%ld\x1",number_of_items);

            withdraw = acc->withdraws;

            while(withdraw)
            {
                fprintf(fout, "%ld\x1%.2f\x1",
                    withdraw->date,
                    withdraw->amount);
                withdraw = withdraw->next;
            }
        }

        /* start next section (services)*/
        service = acc->services;
        number_of_items = 0;
        if(!service)
            fprintf(fout, "%ld\x1",number_of_items);
        else
        {
            while(service){++number_of_items; service=service->next;}
            fprintf(fout, "%ld\x1",number_of_items);

            service = acc->services;

            while(service)
            {
                fprintf(fout, "%d\x1%ld\x1%ld\x1%.2f\x1%ld\x1%s",
                    service->active, service->id, service->acquisition_date,
                    service->unit_price, strlen(service->name), service->name
                    );
                    
                // now deal with payments...
                payment = service->payments;
                number_of_items = 0;
                if(!payment)
                    fprintf(fout, "%ld\x1",number_of_items);
                else
                {
                    while(payment){++number_of_items; payment=payment->next;}
                    fprintf(fout, "%ld\x1",number_of_items);

                    payment = service->payments;

                    while(payment)
                    {
                        fprintf(fout, "%ld\x1%.2f\x1%ld\x1",
                            payment->date, payment->amount, payment->next_payment_date);
                        payment = payment->next;
                    }
                }
                    
                service = service->next;
            }
        }
        
        acc_p = acc_p->next;
    }

    fclose(fout);
    
    // encrypt data after writting it
    
    if(strcmp(
    jermCryptReply=encrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true,false), "Ok"))
    {
        printf("<save_structure> encrypting file: %s\n",jermCryptReply);
        dlclose(libjermCrypt);
        pthread_mutex_unlock(&glock);
        return status;
    }        
    
            
    status = true;
    dlclose(libjermCrypt);

    pthread_mutex_unlock(&glock);

    return status;

}

void gsave()
    // 
{
    if(!GPATHS) return;
    dump_constants(GPATHS->jerm_crypt_path,GPATHS->data_files_path);
    save_structure(GPATHS->jerm_crypt_path,GPATHS->data_files_path);
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

    if(!GLOCK_INITIALISED)
    {
        fprintf(stdout, "FAILED TO load structure. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    //printf("load structure\n");

    bool status = false;

    unsigned int buff_length;

    pthread_mutex_lock(&glock);

    String crypt_lib_paths[] = {jermCrypt_path,"/",JERM_CRYPT_LIB,"\0"};
    String data_file_paths[] = {save_dir, "/",STRUCTURE_FILE,"\0"};
    String jermCryptReply;
    
    length_of_all_strings(crypt_lib_paths, &buff_length);
    char crypt_lib_path[buff_length+1];
    join_strings(crypt_lib_path, crypt_lib_paths);

    length_of_all_strings(data_file_paths, &buff_length);
    char data_file_path[buff_length+1];
    join_strings(data_file_path, data_file_paths);

    FILE *fin = fopen(data_file_path,"rb");

    if(!fin) {pthread_mutex_unlock(&glock); return status;}

    /* load function from jermCrypt library eg; */
    void *libjermCrypt = dlopen(crypt_lib_path, RTLD_GLOBAL|RTLD_LAZY);

    if(!libjermCrypt) {fclose(fin); pthread_mutex_unlock(&glock); return status;}

    /* encrypt_file & decrypt_file prototypes in libjermCrypt.so */
    // char *encrypt_file(char *fin, char *pswd, char *fout, int overwrite, int verbose);
    // char *decrypt_file(char *fin, char *pswd, char *fout, int overwrite, int verbose);

    char *(*encrypt_file)(char*,char*,char*,int,int) = dlsym(libjermCrypt, "encrypt_file");

    char *(*decrypt_file)(char*,char*,char*,int,int) = dlsym(libjermCrypt, "decrypt_file");

    if((!encrypt_file) || (!decrypt_file)) 
        {dlclose(libjermCrypt); fclose(fin); pthread_mutex_unlock(&glock); return status;}

    fclose(fin);

    // decrypt data file before reading from it...
    // decrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true,false);
    
    if(strcmp(
    jermCryptReply=decrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true,false), "Ok"))
    {
        printf("<load_structure> decrypting file to read data: %s\n",jermCryptReply);
        dlclose(libjermCrypt);
        pthread_mutex_unlock(&glock);
        
        DATA_FILE_PRESENT = true;

        return status;
    }
     

    AccountPointer acc_p, p_acc_p; /* p_acc_p = prev ac_p */
    AccountPointer child, p_child; /* p_acc_p = prev ac_p */
    Account acc;
    Commission commission, p_commission, new_commission;
    Investment investment, p_investment, new_investment;
    Withdraw withdraw, p_withdraw, new_withdraw;
    Service service, p_service, new_service;
    Payment payment, p_payment, new_payment;

    Child child_p=NULL, last_child_p=NULL, new_child_p, p_child_p;
    unsigned long number_of_items, child_id, string_index, string_length;
    
    /* perform garbage collection on the current structure*/
    if(HEAD->next)
    {
        acc_p = HEAD->next;
        gfree(HEAD); /* first clear HEAD after getting the first child it points to */
        
        while(acc_p)
        {
            acc = acc_p->account;
            gfree(acc->names);
            
            commission = acc->commissions;
            
            while(commission)
            {
                gfree(commission->reason);
                p_commission = commission;
                commission = commission->next;
                gfree(p_commission);
            }
            
            investment = acc->investments;
            while(investment)
            {
                gfree(investment->package);
                p_investment = investment;
                investment = investment->next;
                gfree(p_investment);
            }

            withdraw = acc->withdraws;
            while(withdraw)
            {
                p_withdraw = withdraw;
                withdraw = withdraw->next;
                gfree(p_withdraw);
            }
            
            child = acc->children;
            while(child)
            {
                p_child = child;
                child = child->next;
                gfree(p_child);
            }
            
            service = acc->services;
            while(service)
            {
                payment = service->payments;
                while(payment)
                {
                    p_payment = payment;
                    payment = payment->next;
                    gfree(p_payment);
                }
                
                p_service = service;
                service = service->next;
                gfree(p_service->name);
                gfree(p_service);
            }
            
            p_acc_p = acc_p;
            acc_p = acc_p->next;
            gfree(acc);
            gfree(p_acc_p);
        }
        
        HEAD = malloc(sizeof(struct account_pointer));

        if(!HEAD) memerror(stdout);

        HEAD->account = NULL;
        HEAD->next = NULL;
        HEAD->prev = NULL;

        TAIL = HEAD;
    }

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
        Account new_account = malloc(sizeof(struct account));
        
        if(!new_account) memerror(stdout);

        new_account->id = id;
        fscanf(fin,"%d\x1",&(new_account->is_consumer));
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

        new_account->withdraws = NULL;
        new_account->last_withdraw = NULL;

        new_account->rank = 0;

        new_account->highest_leg_ranks[0] = 0.0;
        new_account->highest_leg_ranks[1] = 0.0;
        new_account->highest_leg_ranks[2] = 0.0;

        new_account->services = NULL;
        new_account->last_service = NULL;

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
            "%d\x1"
            "%ld\x1"
            "%f\x1"
            "%d\x1"
            ,
            &(new_account->pv), &(new_account->available_balance), &(new_account->total_returns),
            &(new_account->total_redeems), &uplink_id, new_account->leg_volumes,
            &(new_account->leg_volumes[1]), &(new_account->leg_volumes[2]),
            new_account->TVC_levels, &(new_account->TVC_levels[1]), &(new_account->rank),
            new_account->highest_leg_ranks, &(new_account->highest_leg_ranks[1]),
            &(new_account->highest_leg_ranks[2]),
            &(new_account->date), &(new_account->pv_made_in_first_month),
            &(new_account->redeems)
        );
        
        pthread_mutex_unlock(&glock);
        
        new_account->uplink = get_account_by_id(uplink_id);
        
        pthread_mutex_lock(&glock);

        string_length = 0;
        string_index = 0;
        fscanf(fin,"%ld\x1",&string_length);

        new_account->names = malloc(sizeof(char)*(string_length+1));
        if(!(new_account->names)){gfree(new_account); memerror(stdout);}
        while(string_length)
        {
            fscanf(fin,"%c",&(new_account->names[string_index]));
            --string_length;
            ++string_index;
        } new_account->names[string_index]='\0';

        /* temporary direct-children */
        number_of_items = 0;
        fscanf(fin,"%ld\x1",&number_of_items);

        while(number_of_items)
        {
            fscanf(fin,"%ld\x1",&child_id);
            
            new_child_p = malloc(sizeof(struct child));
            if(!new_child_p)
                {gfree(new_account->names); gfree(new_account); memerror(stdout);}
            
            new_child_p->id = child_id;
            new_child_p->uplink_id = id;
            new_child_p->next = NULL;
            
            if(!child_p)
                child_p = new_child_p;
            else
                last_child_p->next = new_child_p;
            
            last_child_p = new_child_p;
            
            --number_of_items;
        }

        /* commissions */
        number_of_items = 0;
        fscanf(fin,"%ld\x1",&number_of_items);

        while(number_of_items)
        {
            new_commission = malloc(sizeof(struct commission));
            if(!new_commission)
            {
                /* perform garbage collection on all malloc'd data */
                gfree(new_account->names);
                /* the created child_pointers (child_p) are left out...LOOK INTO THIS*/
                gfree(new_account);
                memerror(stdout);
            }

            new_commission->date = time(NULL);
            
            new_commission->amount = 0;
            new_commission->reason = NULL;
            new_commission->next = NULL;
            new_commission->prev = NULL;
            
            fscanf(fin,"%ld\x1",&new_commission->date);

            fscanf(fin,"%f\x1",&new_commission->amount);

            string_length = 0;
            string_index = 0;
            fscanf(fin,"%ld\x1",&string_length);

            new_commission->reason = malloc(sizeof(char)*(string_length+1));
            if(!(new_commission->reason))
                {gfree(new_account->names); gfree(new_account); memerror(stdout);}
            
            while(string_length)
            {
                fscanf(fin,"%c",&(new_commission->reason[string_index]));
                --string_length;
                ++string_index;
            } new_commission->reason[string_index]='\0';
            
            if(!(new_account->commissions)) new_account->commissions=new_commission;
            else
            {
                new_account->last_commission->next=new_commission;
                new_commission->prev = new_account->last_commission;
            }
            new_account->last_commission = new_commission;

            --number_of_items;
        }

        /* investments */
        number_of_items = 0;
        fscanf(fin,"%ld\x1",&number_of_items);

        while(number_of_items)
        {
            new_investment = malloc(sizeof(struct investment));
            if(!new_investment)
            {
                /* garbage collection */
                gfree(new_account->names); 
                
                Commission c = new_account->commissions;
                while(c)
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

            new_investment->total_returns_on_creation = 0.0;
            new_investment->skipped_first_payment_day = false;

            fscanf(fin, "%ld\x1%f\x1%f\x1%d\x1%ld\x1%f\x1%d\x1",
                &(new_investment->date), &(new_investment->amount),  
                &(new_investment->total_returns_on_creation),
                &(new_investment->skipped_first_payment_day),
                &(new_investment->package_id), &(new_investment->returns), 
                &(new_investment->months_returned)
            );

            string_length = 0;
            string_index = 0;
            fscanf(fin,"%ld\x1",&string_length);

            new_investment->package = malloc(sizeof(char)*(string_length+1));
            if(!(new_investment->package))
            {
                gfree(new_account->names); 
                
                Commission c = new_account->commissions;
                while(c)
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


            if(!(new_account->investments)) new_account->investments=new_investment;
            else
            {
                new_account->last_investment->next=new_investment;
                new_investment->prev = new_account->last_investment;
            }
            new_account->last_investment = new_investment;
            
            --number_of_items;
        }

        /* withdraws */
        number_of_items = 0;
        fscanf(fin,"%ld\x1",&number_of_items);

        while(number_of_items)
        {                
            new_withdraw = malloc(sizeof(struct withdraw));
            if(!new_withdraw)
            {
                gfree(new_account->names); 
                
                Commission c = new_account->commissions;
                while(c)
                {
                    gfree(c->reason);
                    p_commission = c;
                    c = c->next;
                    gfree(p_commission);
                }
                Investment inv = new_account->investments;
                while(inv)
                {
                    gfree(inv->package);
                    p_investment = inv;
                    inv = inv->next;
                    gfree(p_investment);
                }
                gfree(new_account); 
                memerror(stdout);                
            }

            fscanf(fin,"%ld\x1%f\x1",&(new_withdraw->date), &(new_withdraw->amount));

            new_withdraw->next = NULL;
            new_withdraw->prev = NULL;
                            
            if(!(new_account->withdraws))
                new_account->withdraws = new_withdraw;
            else
            {
                new_withdraw->prev = new_account->last_withdraw;
                new_account->last_withdraw->next = new_withdraw;
            }
            
            new_account->last_withdraw = new_withdraw;

            --number_of_items;
        }

        /* services + payments */
        number_of_items = 0;
        fscanf(fin,"%ld\x1",&number_of_items);
        
        unsigned long number_of_payments=0;

        while(number_of_items)
        {
            new_service = malloc(sizeof(struct service));
            if(!new_service)
                // fuck it. if you get a memory error here, lets just hope the program terminates in memerror!
                memerror(stdout);

            new_service->payments = NULL;
            new_service->last_payment = NULL;

            new_service->next = NULL;
            new_service->prev = NULL;

            fscanf(fin,"%d\x1%ld\x1%ld\x1%f\x1",
                &(new_service->active), &(new_service->id), &(new_service->acquisition_date),
                &(new_service->unit_price)
            );

            string_length = 0;
            string_index = 0;
            fscanf(fin,"%ld\x1",&string_length);

            new_service->name = malloc(sizeof(char)*(string_length+1));
            if(!(new_service->name))
                // fuck it here too. memerror sohuld EXIT the fucking program
                // m soooooo not in the moods to handle memory leaks
                memerror(stdout);
            
            while(string_length)
            {
                fscanf(fin,"%c",&(new_service->name[string_index]));
                --string_length;
                ++string_index;
            } new_service->name[string_index]='\0';

            // payments...
            number_of_payments = 0;
            fscanf(fin,"%ld\x1",&number_of_payments);
            
            while(number_of_payments)
            {
                new_payment = malloc(sizeof(struct payment));
                if(!new_payment)
                    // memerror better EXIT the program otherwise we'll get memory leaks of all kinds
                    memerror(stdout);

                fscanf(fin,"%ld\x1%f\x1%ld\x1",
                    &(new_payment->date), &(new_payment->amount), &(new_payment->next_payment_date)
                );

                new_payment->next = NULL;
                new_payment->prev = NULL;
                                
                if(!(new_service->payments))
                    new_service->payments = new_payment;
                else
                {
                    new_payment->prev = new_service->last_payment;
                    new_service->last_payment->next = new_payment;
                }
                
                new_service->last_payment = new_payment;

                --number_of_payments;
            }


            if(!(new_account->services))
                new_account->services = new_service;
            else
            {
                new_service->prev = new_account->last_service;
                new_account->last_service->next = new_service;
            }
            
            new_account->last_service = new_service;

            --number_of_items;
        }

        /* reconstruct actual direct children... */
        acc_p = malloc(sizeof(struct account_pointer));
        if(!acc_p)
        {
            gfree(new_account->names); 
            
            Commission c = new_account->commissions;
            while(c)
            {
                gfree(c->reason);
                p_commission = c;
                c = c->next;
                gfree(p_commission);
            }
            Investment inv = new_account->investments;
            while(inv)
            {
                gfree(inv->package);
                p_investment = inv;
                inv = inv->next;
                gfree(p_investment);
            }
            Withdraw w = new_account->withdraws;
            while(w)
            {
                p_withdraw = w;
                w = w->next;
                gfree(p_withdraw);
            }

            gfree(new_account); 
            memerror(stdout);                
        }
        acc_p->account = new_account;
        acc_p->next=NULL;
        acc_p->prev=NULL;

        if(!(HEAD->next)) HEAD->next=acc_p;
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
    while(child_p)
    {

        pthread_mutex_unlock(&glock);

        uplink_account = get_account_by_id(child_p->uplink_id);
        child_account = get_account_by_id(child_p->id);

        pthread_mutex_lock(&glock);
        
        acc_p = malloc(sizeof(struct account_pointer));
        if(!acc_p)
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
        
        if(!(uplink_account->children)) uplink_account->children=acc_p;
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
    
    // encrypt file adter reading from it
    
    if(strcmp(
    jermCryptReply=encrypt_file(data_file_path, JERM_CRYPT_PASSWORD, data_file_path, true,false), "Ok"))
    {
        printf("<load_structure> encrypting file after reading data: %s\n",jermCryptReply);
        dlclose(libjermCrypt);
        pthread_mutex_unlock(&glock);
        return status;
    }
    
            
    status = true;
    dlclose(libjermCrypt);

    pthread_mutex_unlock(&glock);

    return status;
}

bool update_monthly_auto_refill_percentages(float auto_refill_percentages[][2], String jermCrypt_path, String save_dir)
{

    if(!GLOCK_INITIALISED)
    {
        fprintf(stdout, "FAILED TO update M-A-Rs. glock NOT INITIALISED. did you call init?"); 
        return false;
    }
    
    //printf("update M-A-R-Ps\n");

    bool status = false;
    
    if(!auto_refill_percentages) return status;

    unsigned int num_of_arps=0;
    for(; auto_refill_percentages[num_of_arps][0]; ++num_of_arps);
    
    num_of_arps ? ++num_of_arps : num_of_arps;
    
    if(num_of_arps)
    {
        pthread_mutex_lock(&glock);
        
        if(MONTHLY_AUTO_REFILL_PERCENTAGES)
        {
            bool deleting = true;
            for(int i=0; deleting; ++i)
            {
                if(!MONTHLY_AUTO_REFILL_PERCENTAGES[i][0]) deleting = false;
                gfree(MONTHLY_AUTO_REFILL_PERCENTAGES[i]);
            }
            gfree(MONTHLY_AUTO_REFILL_PERCENTAGES);
        }

        MONTHLY_AUTO_REFILL_PERCENTAGES = malloc(num_of_arps*sizeof(float*));
        if (!MONTHLY_AUTO_REFILL_PERCENTAGES) memerror(stdout);
        for(unsigned int i=0; i<num_of_arps; ++i)
        {
            MONTHLY_AUTO_REFILL_PERCENTAGES[i] = malloc(sizeof(float)*2);
            if(!MONTHLY_AUTO_REFILL_PERCENTAGES[i]) memerror(stdout);
            
            MONTHLY_AUTO_REFILL_PERCENTAGES[i][0] = auto_refill_percentages[i][0]; 
            MONTHLY_AUTO_REFILL_PERCENTAGES[i][1] = auto_refill_percentages[i][1];
        }

        pthread_mutex_unlock(&glock);
        
        status = dump_constants(jermCrypt_path, save_dir);
    
    }

    return status;
}

bool search_investments(time_t search_from, time_t search_to, 
    unsigned int months_returned_from, unsigned int months_returned_to,
    String type, String fout_name)
{
    /* _fout will be a json file 
        
        - if search_from or search_to is 0, search will not be based on investment date.
        - type is either "all" or one of the investment packages ie Saphire, Ruby or Diamond
        
        fout_name: json file where results will be dumped in format 
            {'results':[[date,pkg-name,amount,months-returned],...]}
    */

    FILE *fout = fopen(fout_name,"w");
    if(!fout) 
        return false;

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO search investments. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    bool status = false;

    bool search_by_date = (!search_from) || (!search_to) ? false : true;
    bool search_by_type = strcmp(type,"all") ? true : false;
    bool search_by_months_returned = (!months_returned_from) || (!months_returned_to) ? false : true;

    if(search_by_date && search_from>=search_to)
    {
        fprintf(fout,"invalid <search_from-search_to> data provided. <search_from> must be lesser than <search_to>");
        fclose(fout);
        return status;
    }
    if(search_by_months_returned && 
        ((months_returned_from>=months_returned_to) || (months_returned_to>12))
    )
    {
        fprintf(fout,"invalid <months_returned_from-months_returned_to> data provided."
                     " <months_returned_from> must be lesser than <months_returned_to>"
                     " and <months_returned_to> must be <=12");
        fclose(fout);
        return status;
    }

    fprintf(fout,"{\"results\":[");

    if(HEAD->next)
    {
        AccountPointer ap = HEAD->next;
        Account acc;
        Investment inv;
        
        bool started_writting = false;
        
        struct tm *lt;
        
        while(ap)
        {
            acc = ap->account;
            inv = acc->investments;
            while(inv)
            {
                if(
                    (search_by_date && ((inv->date<search_from) || (inv->date>search_to))) ||
                    (search_by_type && strcmp(type,inv->package)) ||
                    (search_by_months_returned &&
                    ((inv->months_returned<months_returned_from)||(inv->months_returned>months_returned_to)))
                )
                {
                    inv = inv->next;
                    continue;
                }

                // if all search criteria is passed...
                // write data as: []

                if (started_writting)
                    fprintf(fout, ",");
                started_writting = true;

                lt = localtime(&(inv->date));
                fprintf(fout, "[\"%d/%d/%d\", \"%s\", %.2f, %d]",
                    lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900),
                    inv->package, inv->amount/POINT_FACTOR, inv->months_returned
                );

                /*
                fprintf(stdout, " [\"%d/%d/%d\", %s, %.2f, %d]\n",
                    lt->tm_mday,(lt->tm_mon+1),(lt->tm_year+1900),
                    inv->package, inv->amount/POINT_FACTOR, inv->months_returned
                );
                */
                
                inv = inv->next;
            }
            
            ap = ap->next;
        }
    }

    fprintf(fout,"]}");

    fclose(fout);
    status = true;
    return status;
}


void get_next_payment_date(const time_t *current_date, time_t *next_date, const unsigned short int months)
{
    /* push current_day n months infront. eg if today is 13/02/2017 and we push the day 5 months infront,
       the new date will be 13/07/2017
    */
    struct tm *lt;
    unsigned short int month_day_today;

    lt = localtime(current_date);
    month_day_today = lt->tm_mday;

    *next_date = (*current_date) + (GHFU_MONTH*months);

    lt = localtime(next_date);
        
    *next_date = (*next_date) - (GHFU_DAY*(lt->tm_mday-month_day_today));
}

void set_service_aquisition_date(time_t *date)
{
    /* if date->day > 28, aquisition_date is set to 1'st. this date directly coresponds to the day the
       service is to be paid for...
     */
    struct tm *lt;
    lt = localtime(date);
    while (lt->tm_mday>28)
    {
        *date += GHFU_DAY;
        lt = localtime(date);
    }
}

bool upgrade_account(Account account, Account new_uplink, const Amount amount, bool test_feasibility, FILE *fout)
{
    /* upgrage account from consumer account to ghfu member-account */

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO upgrade account. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    if(!account) {ghfu_warn(28,fout); return false;}
    if(!(account->is_consumer))
    {
        fprintf(fout, "ACCOUNT PROVIDED IS NOT A CONSUMER ACCOUNT"); 
        return false;
    }
    if(amount<ACCOUNT_CREATION_FEE) { ghfu_warn(1,fout); return false; }
    if(amount>(ACCOUNT_CREATION_FEE+MAXIMUM_INVESTMENT+OPERATIONS_FEE)) { ghfu_warn(9,fout); return false; }

    AccountPointer consumer_node = account->uplink->children;

    if(new_uplink)
        // locate node corresponding to consumer_account in the system-account's children
    {
        while(consumer_node && ((Account)(consumer_node->account)!=account))
            consumer_node = consumer_node->next;
        if(!consumer_node || (Account)(consumer_node->account)!=account)
        {
            ghfu_warn(33,fout);            
            return false;
        }
    }


    float _amount = amount - ACCOUNT_CREATION_FEE;

    pthread_mutex_lock(&glock);
    account->is_consumer = false;
    pthread_mutex_unlock(&glock);

    if (_amount)
    {
        if(!invest_money(account, _amount, false, true, fout))
        {
            pthread_mutex_lock(&glock);
            account->is_consumer = true;
            pthread_mutex_unlock(&glock);
            
            return false;
        }
    }

    if(test_feasibility)
    {
        pthread_mutex_lock(&glock);
        account->is_consumer = true;
        pthread_mutex_unlock(&glock);
        
        return true;
    }

    if(new_uplink && new_uplink!=account->uplink)
        // transfer node from system account as uplink to new uplink
    {
        pthread_mutex_lock(&glock);

        if(!(consumer_node->prev)) account->uplink->children = consumer_node->next;
        else consumer_node->prev->next = consumer_node->next;
        if (!(consumer_node->next)) account->uplink->last_child = consumer_node->prev;

        if(!(new_uplink->children)) 
        {
            new_uplink->children = consumer_node;
            consumer_node->prev = NULL;
            consumer_node->next = NULL;
        }
        else
        {
            consumer_node->prev = new_uplink->last_child;
            new_uplink->last_child->next = consumer_node;
            consumer_node->next = NULL;
        }
        new_uplink->last_child = consumer_node;
        
        account->uplink = new_uplink;

        pthread_mutex_unlock(&glock);

    }

    invest_money(account, _amount, false, false, fout);

    pthread_mutex_lock(&glock);
    SYSTEM_FLOAT += amount;
    pthread_mutex_unlock(&glock);

    return true;
}

bool upgrade_consumer_account(ID account_id, ID new_uplink_id, const Amount amount, bool test_feasibility, String fout_name)
{
    // if uplink is still the system account, uplink_id=0. this is the same as providing an invalid uplink_id
    bool status = false;
    
    FILE *fout = fopen(fout_name, "w");
    if(!fout) return status;
    
    Account account = get_account_by_id(account_id), uplink = get_account_by_id(new_uplink_id);
    
    status = upgrade_account(account,uplink,amount,test_feasibility,fout);
    
    fclose(fout);
    return status;
}

bool create_new_service(Account account, const ID service_id, const String service_name, const Amount unit_price, 
    const Amount amount, const bool test_feasibility, FILE *fout)
{
    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO create new service. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    if(!account) {ghfu_warn(28,fout); return false;}

    if(unit_price<0 || amount<0) {ghfu_warn(36,fout); return false;}

    if(account->services)
    {
        Service s = account->services;
        while(s)
        {
            if(s->id==service_id) {ghfu_warn(35,fout); return false;}
            s = s->next;
        }
    }

    Service new_service = malloc(sizeof(struct service));
    if(!new_service) memerror(fout);

    new_service->id = service_id;    
    new_service->active = true;

    new_service->name = NULL;

    new_service->unit_price = unit_price;
    
    new_service->payments = NULL;
    new_service->last_payment = NULL;

    new_service->next = NULL;
    new_service->prev = NULL;

    new_service->acquisition_date = time(NULL);
    set_service_aquisition_date(&(new_service->acquisition_date));

    pthread_mutex_lock(&glock);
    if (!(account->services)) account->services = new_service;
    else
    {
        new_service->prev = account->last_service;
        account->last_service->next = new_service;
    }
    
    account->last_service = new_service;
    pthread_mutex_unlock(&glock);

    if(amount && !pay_for_service(account,service_id,amount,true,fout)) 
    {
        if(!(account->last_service->prev))
            {account->services=NULL; account->last_service=NULL;}
        else
            {account->last_service = account->last_service->prev; account->last_service->next=NULL;}
        
        gfree(new_service); 
        return false;
    }

    if(test_feasibility) 
    { 
        if(!(account->last_service->prev))
            {account->services=NULL; account->last_service=NULL;}
        else
            {account->last_service = account->last_service->prev; account->last_service->next=NULL;}
        
        gfree(new_service); 
        return true;        
    }

    unsigned int buff_length;
    String name_strings[] = {service_name, "\0"};
    length_of_all_strings(name_strings, &buff_length);

    new_service->name = malloc(sizeof(char)*(buff_length+1));
    if(!(new_service->name)) {gfree(new_service); memerror(fout);}
    join_strings(new_service->name,name_strings);

    if(amount)
        pay_for_service(account,service_id,amount,false,fout);

    return true;
}

bool register_new_service(ID account_id, const ID service_id, const String service_name, const Amount unit_price, 
    const Amount amount, const bool test_feasibility, String fout_name)
{
    bool status = false;
    
    FILE *fout = fopen(fout_name, "w");
    if(!fout) return status;

    Account account = get_account_by_id(account_id);    
    status = create_new_service(account, service_id, service_name, unit_price, amount, test_feasibility, fout);
    
    fclose(fout);
    return status;
}

bool pay_for_service(Account account,const ID service_id, const Amount amount,const bool test_feasibility, FILE *fout)
{
     if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO create new service. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    if(!account) {ghfu_warn(28,fout); return false;}
    if(!amount) {fprintf(fout,"PLEASE PROVIDE MONEY TO CLEAR THE SERVICE"); return false;}
    if(amount<0) {ghfu_warn(36,fout); return false;}
    
    pthread_mutex_lock(&glock);
    Service service = account->services;

    while(service)
    {
        if(service->id==service_id) break;
        service = service->next;
    }
    
    pthread_mutex_unlock(&glock);

    if(!service){ghfu_warn(31, fout); return false;}
    if(!(service->active)){ghfu_warn(34, fout); return false;}
    if (amount < service->unit_price){ghfu_warn(32, fout); return false;}

    if(test_feasibility) return true;

    pthread_mutex_lock(&glock);

    unsigned short int months_paid = amount/service->unit_price; // any slight excess WILL NOT be considered
    Amount points = amount*POINT_FACTOR;

    time_t payment_day;
    if(!(service->payments)) payment_day = service->acquisition_date;
    else payment_day = service->last_payment->next_payment_date;

    pthread_mutex_unlock(&glock);

    Payment new_payment = malloc(sizeof(struct payment));
    if(!new_payment) memerror(fout);

    new_payment->date = time(NULL);

    new_payment->amount = amount;

    new_payment->next = NULL;
    new_payment->prev = NULL;

    char points_str[16];
    unsigned int buff_length;

    if (new_payment->date<(payment_day+(GHFU_DAY*10)))
        // qualifies to get the consumer-rebet
    {
        Amount comm = new_payment->date <= payment_day ?
            points*CONSUMER_REBETS[0]*0.01 : points*CONSUMER_REBETS[1]*0.01;

        sprintf(points_str," $%.2f",points);

        String c_reason_strings[] = {"REBET from paying for service worth",points_str," points", "\0"};
        length_of_all_strings(c_reason_strings, &buff_length);
        char c_reason[buff_length+1];
        join_strings(c_reason, c_reason_strings);
        award_commission(account, comm, "REBET", c_reason, fout);

    }

    get_next_payment_date(&payment_day, &(new_payment->next_payment_date), months_paid);

    pthread_mutex_lock(&glock);

    if(!(service->payments)) 
        service->payments = new_payment;
    else
    {
        service->last_payment->next = new_payment;
        new_payment->prev = service->last_payment;
    }
    service->last_payment = new_payment;

    pthread_mutex_unlock(&glock);

    // award IBC n QSB
    if (!(account->is_consumer))
    {
        sprintf(points_str," $%.2f",points);

            // IBC
        String c_reason_strings_[] = {"IBC from buying yourself a service/package worth",points_str," points","\0"};
        length_of_all_strings(c_reason_strings_, &buff_length);
        char c_reason_[buff_length+1];
        join_strings(c_reason_, c_reason_strings_);
        award_commission(account, points, "IBC", c_reason_, fout);

            // QSB
        String c_reason_strings__[] = {"QSB from ", account->names,"'s purchase of a package worth", points_str," points","\0"};
        length_of_all_strings(c_reason_strings__, &buff_length);
        char c_reason__[buff_length+1];
        join_strings(c_reason__, c_reason_strings__);
        award_commission(account->uplink, points, "IBC", c_reason__, fout);

    }
    
    increment_pv(account,points,fout);

    pthread_mutex_lock(&glock);
    SYSTEM_FLOAT += amount;
    pthread_mutex_unlock(&glock);
    
    return true;
}

bool pay_for_consumer_service(ID account_id, const ID service_id, const Amount amount,const bool test_feasibility, const String fout_name)
{
    bool status = false;
    
    FILE *fout = fopen(fout_name, "w");
    if(!fout) return status;

    Account account = get_account_by_id(account_id);    
    status = pay_for_service(account, service_id, amount, test_feasibility,fout);
    
    fclose(fout);
    return status;

}

bool change_service_status(Account account, ID service_id, bool new_status, FILE *fout)
{
     if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO create new service. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    if(!account) {ghfu_warn(28,fout); return false;}
    
    pthread_mutex_lock(&glock);
    Service service = account->services;

    while(service)
    {
        if(service->id==service_id) break;
        service = service->next;
    }
    
    pthread_mutex_unlock(&glock);

    if(!service){ghfu_warn(31, fout); return false;}

    service->active = new_status;

    return true;
}

bool update_service_status(const ID account_id, const ID service_id, const bool new_status, const String fout_name)
{
    bool status = false;
    
    FILE *fout = fopen(fout_name, "w");
    if(!fout) return status;

    Account account = get_account_by_id(account_id);    
    status = change_service_status(account, service_id, new_status, fout);
    
    fclose(fout);
    return status;

}

bool search_services(const String consumer_name, const String service_name, 
    const time_t service_acquisition_date, const bool service_paid, 
    const bool service_active, const bool security_month_paid, const String fout_name)
{
    /*
        consumer_name: "all" will search through all members, otherwise specified member will be searched
        service_name: "all" will search through all serives, otherwise specified service will be searched
        service_acquisition_date: return services created on or after this date. 0 will return all
        service_paid: 1/0
        sercive_active: 1/0 .this is used to for example search for empty houses/room
        security_month_paid: 1/0
        fout_name: file where json data will be dumped in format
            { "item-number":
                [consumer-name,service-name,service-id,service-unit-price,last-payment-day,next-payment-day],
                ...
            }


    */
    FILE *fout = fopen(fout_name, "w");
    if(!fout) return false;

    if(!GLOCK_INITIALISED)
    {
        fprintf(fout, "FAILED TO search investments. glock NOT INITIALISED. did you call init?"); 
        return false;
    }

    bool search_specific_consumer = strcmp(consumer_name,"all") ? true : false;
    bool search_specific_service = strcmp(service_name,"all") ? true : false;

    fprintf(fout, "{");

    AccountPointer ap = HEAD->next;
    Service service;
    
    bool started_writting=false;
    bool service_currently_paid=false;
    bool service_has_security_month=false;
    
    unsigned long number_of_results = 1;
    
    time_t today; today = time(NULL);
    struct tm *lpd, *npd; // next and last payment-day

    while(ap)
    {
        if(
            (search_specific_consumer && strcmp(((Account)(ap->account))->names, consumer_name))
        )
        {
            ap = ap->next;
            continue;
        }

        service = ((Account)(ap->account))->services;
        
        while(service)
        {
            if(
                (service_acquisition_date > service->acquisition_date) ||
                (search_specific_service && strcmp(service->name, service_name)) ||
                ((service_active!=neutral) && (service_active != service->active))
            )
            {
                service = service->next;
                continue;                
            }
            
            service_currently_paid = !(service->payments) ? false : 
                (today <= (service->last_payment->next_payment_date) ? true : false);
            
            service_has_security_month = !service_currently_paid ? false : 
                ((service->last_payment->next_payment_date)-today > GHFU_MONTH ? true : false);
            
            if (((service_paid!=neutral) && (service_currently_paid!=service_paid)) || 
                ((security_month_paid!=neutral) && (service_has_security_month!=security_month_paid))
            )
            {
                service = service->next;
                continue;                
            }

            // when all search criteria is matched...
            if (started_writting)
                fprintf(fout, ",");
            started_writting = true;

            fprintf(fout,
                "\"%ld\":[\"%s\",\"%s\",%ld,%.2f,",
                number_of_results,
                ((Account)(ap->account))->names, service->name, service->id,
                service->unit_price
            );

            if(!(service->payments))
                fprintf(fout,"\"%s\",\"%s\"]","---","---");
            else
            {
                lpd = localtime(&(service->last_payment->date));
                npd = localtime(&(service->last_payment->next_payment_date));

                fprintf(fout,"\"%d/%d/%d\",\"%d/%d/%d\"]",
                    lpd->tm_mday,(lpd->tm_mon+1),(lpd->tm_year+1900),
                    npd->tm_mday,(npd->tm_mon+1),(npd->tm_year+1900)
                );
            }

            ++number_of_results;
            service = service->next;
        }

        ap = ap->next;
    }

    fprintf(fout, "}");

    fclose(fout);

    return true;
}
