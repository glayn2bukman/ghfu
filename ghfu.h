/*
    Project name: GHFU (GoodHomesForYou)
    Code name: shadow wallet
    Logic: all explained in the proposal document (see bukman or richard about this)
    Owner: JERM Technology
    License: Private; all rights reserved by JERM technology

    RECURSION DESIGN: i always use iteration over recurssion due to the function-call overhead issue
                      in recurssion. a project like this that may perform millions of recursice-like
                      calculations would get considerably slow if recursion was used instead of 
                      iteration

    NB: this program is thread-safe so no pressure(?) on whoever is calling libjermGHFU.so from the server 
        program.
        the url <http://www.thegeekstuff.com/2012/05/c-mutex-examples/?refcom> is a nice place to see the logic
        used here(mutex locks)


        DONT FUCK WITH THE pthread_mutex_[un]lock(&glock) CALLS. THEY ARE THE BASIS OF ALL POSSIBLE BUGS
        INTRODUCED BY MULTI-THREADING. TAKE YOUR TIME TO FULLY UNDERSTAND WHY EACH CALL IS
        PRECICELY WHERE IT IS AND NO A LINE AFTER OR BEFORE! IF THESE CALLS ARE MESSED WITH, YOU GOT 
        YOURSELF A DEAD-LOCKED PROGRAM (ONE THREAD/FUNCTION LOCKS A mutex AND DOESN'T UNLOCK IT (maybe 
        it exited ubruptly, or called another function that needs a variable locked by the function calling 
        the new function eg invest_money calls award_commission at one point an in this case, care must be
        takn to ensure that invest_money only calls award_commission after ensuring any variable needed by
        award_commission is unlocked...simpler said than done :):):) )) 

        on my development machine (Dell-Inspiron-3542), the maximum number of threads a process could 
        dispatch is 30503 so my python server could handle a maximum of 30503 concurrent connections
        thus dispatching a maximum of 30503 threads to libjermGHFU.so
*/

/* library inclusion */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<dlfcn.h>

#include<pthread.h>


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
    ID id;
    String names;
    Amount pv; /* Personal Volume (the backbone of all commissions and bonuses) */
    Amount available_balance;
    Amount total_returns;
    Amount total_redeems;

    struct account *uplink;
    AccountPointer children;
    AccountPointer last_child;

    Commission commissions;
    Commission last_commission;

    Amount leg_volumes[3];
    Amount TVC_levels[2]; /* team-volume-commission levels. 
                         always subtract this value from the lower leg to compute the TVC.
                         This way, we accomplish the "leveling off" of TVC legs everytime
                         TVCs are calculated. since TVC is for only the first 2 legs, we need another
                         TVC_level to be used in determining rank-related bonuses
                      */

    Investment investments;
    Investment last_investment;

    unsigned int rank;
    unsigned int highest_leg_ranks[3];

} *Account;

typedef struct child
    /* is used to represent account children when loading structure from disk. at the time of an
       account reconstruction, there are no children yet(because children come after the account
       as the structure is stored in linear order or account creation ie HEAD->next->next->...TAIL)
       hence we can only properly allocate 
    */
{
    ID id;
    ID uplink_id;   
    struct child *next;
} *Child;

Amount SYSTEM_FLOAT, CUMULATIVE_COMMISSIONS, COMMISSIONS;
ID ACTIVE_ACCOUNTS, CURRENT_ID;

AccountPointer HEAD, TAIL;

/* function prototypes */
void memerror(FILE *fout);
void init(String jermCrypt_path, String save_dir);
bool increment_pv(Account account, const Amount points, FILE *fout);
bool award_commission(Account account, const Amount points, const String commission_type, String reason, FILE *fout);
bool invest_money(Account account, const Amount amount, const bool update_system_float, FILE *fout);
bool invest(ID account_id, const Amount amount, const bool update_system_float, String fout_name);
Account register_member(Account uplink, String names, Amount amount, FILE *fout);
ID register_new_member(ID uplink_id, String names, Amount amount, String fout_name);

bool buy_property(Account IB_account, const Amount amount, FILE *fout);
bool purchase_property(ID IB_account_id, const Amount amount, String fout_name);
bool auto_refill(Account account, FILE *fout);
bool raise_rank(Account account, FILE *fout);
bool calculate_tvc(Account account, FILE *fout);
bool award_rank_monthly_bonuses(Account account, FILE *fout);

/* functions to visually display accounts */
void show_commissions(const Account account);
void show_leg_volumes(const Account account);
void show_investments(const Account account);
void show_direct_children(const Account account);
void structure_details(const Account account); 

/* functions to dump account information to secondary interfaces accessed by other APIs eg from Python */
bool dump_commissions(const Account account, FILE *fout);
bool dump_leg_volumes(const Account account, FILE *fout);
bool dump_investments(const Account account, FILE *fout);
bool dump_direct_children(const Account account, FILE *fout);
bool dump_structure_details(ID account_id, String fout_name); 

bool redeem_points(Account account, Amount amount, FILE *fout);
bool redeem_account_points(ID account_id, Amount amount, String fout_path);

void length_of_all_strings(String strings[], unsigned int *length);
void join_strings(char buff[], const String strings[]);

Account get_account_by_id(const ID id);
ID account_id(Account account);

bool monthly_operations(FILE *fout);
bool perform_monthly_operations(String fout_name);

void ghfu_warn(unsigned int ghfu_errno,FILE *fout);
void gfree(void *p);

/* data constants modifiers*/
bool set_constant(String constant, Amount value);

bool dump_constants(String jermCrypt_path, String save_dir);
bool save_structure(String jermCrypt_path, String save_dir);

    /* the load constants are meant to be used ONLY from init, which in turn should ONLY BE CALLED ONE AT 
       SYSTEM START. none the less, load_structure implements a 'current-data-erase' scheme to avoid memory
       leaks in case load_structure is called outside init or if init is called more than once. y got the extra
       mile? well, because people are NOT good at reading documentation and more so following it! 
    */
bool load_constants(String jermCrypt_path, String save_dir);
bool load_structure(String jermCrypt_path, String save_dir);

bool update_monthly_auto_refill_percentages(float auto_refill_percentages[][2], String jermCrypt_path, String save_dir);

/* thread-safe variables */
pthread_mutex_t glock;
