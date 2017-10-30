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
*/

/* library inclusion */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

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
    Amount TVC_level; /* team-volume-commission level. 
                         always subtract this value from the lower leg to compute the TVC.
                         This way, we accomplish the "leveling off" of TVC legs everytime
                         TVCs are calculated
                      */

    Investment investments;
    Investment last_investment;

    unsigned int rank;
    unsigned int highest_leg_ranks[3];

} *Account;

Amount SYSTEM_FLOAT, CUMULATIVE_COMMISSIONS, COMMISSIONS;
ID ACTIVE_ACCOUNTS, CURRENT_ID;

AccountPointer HEAD, TAIL;

/* function prototypes */
void memerror();
void init();
void increment_pv(Account account, const Amount points);
void award_commission(Account account, const Amount points, const String commission_type, String reason);
bool invest_money(Account account, const Amount amount, const String package, const ID package_id, const bool update_system_float);
Account register_member(Account uplink, String names, Amount amount);

void buy_property(Account IB_account, const Amount amount, const bool member, const String buyer_names);
void auto_refill(Account account, float percentages[][2]);
bool raise_rank(Account account);
void calculate_tvc(Account account);

void show_commissions(const Account account);
void show_leg_volumes(const Account account);
void show_investments(const Account account);
void show_direct_children(const Account account);
void structure_details(const Account account); 

bool redeem_points(Account account, Amount amount);

void length_of_all_strings(String strings[], unsigned int *length);
void join_strings(char buff[], const String strings[]);

Account get_account_by_id(const ID id);

void ghfu_warn(unsigned int ghfu_errno);
void gfree(void *p);

