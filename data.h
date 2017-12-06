/*
This header file contains the primary DATA values for the project GHFU
*/

/* the jermCrypt library */
char *JERM_CRYPT_LIB = "libjermCrypt.so";
char *DATA_FILE = "data.ghfu";
char *STRUCTURE_FILE = "structure.ghfu";
char *JERM_CRYPT_PASSWORD = "j<ermG.hfuCrypt>";

/* the global-lock checker (the GL ensures that we have a thread-safe program)*/
bool GLOCK_INITIALISED = false;

/* the conversion factor from amount($) to points */
#define TBB_MAX_GENERATIONS 7

float POINT_FACTOR = 0.25;
unsigned int PAYMENT_DAY = 28; /* any day from 1'st to 28'th (29+ may be absent in some months...) */
unsigned int LAST_INVESTMENT_DAY = 14; /* we can only get investment money on so many days in a given month */

/* account fees($) they sohuld tally up to $400.0*/
float ACCOUNT_CREATION_FEE = 16.0; /* $ */
float ANNUAL_SUBSCRIPTION_FEE = 0.0; /* $ */
float OPERATIONS_FEE = 400.0; /* $ */

float MINIMUM_INVESTMENT = 300.0; /* 75 points */
float MAXIMUM_INVESTMENT = 700.0; /* 175 points */

/* investment plans */
float INVESTMENTS[4]={
    // if you edit these values, edit MINIMUM_INVESTMENT & MAXIMUM_INVESTMENT
    300.0,500.0,700.0,
    0.0 /* terminating condition */
};

/* exchange rate related variables */
int RATE_INFLATE = 50;
int EXCHANGE_RATE=3600; /* 
                        deposit rate is always <RATE_INFLATE> more while withdraw value is always 
                        RATE_INFLATE less.
                        also, this value can be modified anytime using uri </update_exchange_rate> but once
                        every half-day, the server will first attempt to collect the latest value at
                        <http://usd.fxexchangerate.com/ugx/> and in case it fails, then the manually set
                        value will be used!
                    */
float WITHDRAW_CHARGE = 9.0; // this charge is the percentage GHFU charges a client when they withdraw funds 


/* program errors */
char *ERRORS[] = {
    /*0*/ "MALLOC FAILED TO ALOCATE SPACE",
    /*1*/ "INSUFICIENT FUNDS TO CREATE NEW ACCOUNT",
    /*2*/ "INSUFFICIENT FUNDS TO INVEST",
    /*3*/ "FAILED TO AWARD IBC, IB ACCOUNT is NULL",
    /*4*/ "INVESTMENT MONEY DOES NOT INCLUDE OPERATIONAL COST. INVESTMENT DUMPED",
    /*5*/ "INVESTMENT OF $0 is not permissible! INVESTMENT DUMPED",
    /*6*/ "INVESTMENT BELOW MINIMUM. INQUIRE WITH MANAGEMENT ABOUT THIS!",
    /*7*/ "CANT INCREMENT PV FOR NULL ACCOUNT",
    /*8*/ "CANT GIVE ANY COMMISSIONS FOR 0 POINTS",
    /*9*/ "AMOUNT EXEEDS MAXIMUM INVESTABLE AMOUNT. INQUIRE WITH MANAGEMENT ABOUT THIS!",
    /*10*/ "INSUFFICIENT FUNDS. REDEEM OPERATION DUMPED",
    /*11*/ "CANT REDEEM POINTS FROM NULL ACCOUNT!",
    /*12*/ "FAILED TO PROCESS PAYMENT, NOT PAYMENT DAY.  INQUIRE WITH MANAGEMENT ABOUT THIS!",
    /*13*/ "VERY LOW AMOUNT WAS SOMEHOW INVESTED. PLEASE LOOK INTO THIS",
    /*14*/ "TVC ONLY AVAILABLE FOR MEMBERS ATLEAST 2 LEGS, EACH WITH ATLEAST ONE DOWNLINK!",
    /*15*/ "FAILED TO CALCULATE TVC, NOT CALCULATION DAY.  INQUIRE WITH MANAGEMENT ABOUT THIS!",
    /*16*/ "PLEASE SET THE MONTHLY-AUTO-REFILL-PERCENTAGES",
    /*17*/ "PACKAGE/SERVICE OF $0 is NOT SOLEDBY THE COMPANY",
    /*18*/ "INVESTMENTS ARE DESCRET AND SET TO ONLY A FEW SPECIFIC VALUES. INQUIRE WITH ADMIN ABOUT THIS!",
    /*19*/ "TODAY IS PAST THE LAST INVESTMENT DAY. CONSULT WITH ADMIN ABOUT THIS PLEASE",
    /*20*/ "FAILED TO GIVE MONTHY-RANK-BONUSES. NOT PAYMENT DAY",
    /*21*/ "TOTAL RETURNS SINCE INVESTMENT EXCEED MAXIMUM PROFIT ALLOWED FOR INVESTMENT",
    /*22*/ "RETURNS TRANCATED TO NOT EXCEED MAXIMUM ALLOWABLE INVESTMENT PROFIT",
    /*23*/ "RETURNS TOPPED UP TO AQUIRE MINIMUM ALLOWABLE INVESTMENT PROFIT",
    /*24*/ "THIS ACCOUNT BELONGS TO THE SYSTEM> YOU CANT REDEEM ITS POINTS",
};

/* montlhy auto-refill percentages (defaults, lower limits)
   this will be allocated space from malloc
    default values;
        {
            {375, 40},
            {250, 40},
            {125, 40},
            {0, 0}
        };
*/
float **MONTHLY_AUTO_REFILL_PERCENTAGES = NULL;

/* account types */
char *ACCOUNTS[] = {
    "Independent Brocker",
    "Saphire",
    "Ruby",
    "Diamond",
    "Unknown" /* terminating condition */
};

char *RANKS[] = {
    /*0*/ "Independent Brocker",
    /*1*/ "Consultant",
    /*2*/ "Junior Manager",
    /*3*/ "Manager",
    /*4*/ "Senior Manager",
    /*5*/ "Director",
    /*6*/ "Senior Director",
    /*7*/ "Executive Director",
    /*8*/ "Senior Executive Director",
    /*9*/ "President",
    /*10*/ "Chairman",
    /*11*/ "Crown",
    /*12*/ "\0", /* termination condition*/
};

float RANK_DETAILS[][4] = {
    /* volumes required to reach a certain rank. order;
        higest rank in any of the legs, PV, cummulative organisation volume, award
         
    */
    /*0*/  {0,  0,    0,        0},
    /*1*/  {0,  60,   1000,     0},
    /*2*/  {0,  80,   3000,     0},
    /*3*/  {0,  100,  10000,    0},
    /*4*/  {0,  120,  30000,    0},
    /*5*/  {0,  140,  100000,   500},
    /*6*/  {0,  160,  300000,   1000},
    /*7*/  {0,  180,  1000000,  2500},
    /*8*/  {7,  0,    0,        6000},
    /*9*/  {8,  0,    0,        15000},
    /*10*/ {9,  0,    0,        25000},
    /*11*/ {10, 0,    0,        50000},
    
    /*12*/ {0,  1,    1,        0} /* terminating condition */
};

/* commission/bonus ranges (all amounts in points NOT $)*/

float IBC[][2] = { /* independent-brocker-commissions/discounts */
    /* array-values are in format {PV+, %discount/commission} */
    {0, 2},
    {70, 5},
    {120, 5},
    {170, 5},
    {0, 0} /* terminating condition */
};

float INVESTMEN_SCHEME[][5] = {
    /* array-values are in format {points+, %lowest, %highest, %min-profit-returns, %max-profit-returns} 
       the data is in descending investment-points order
    */
    {175, 40, 90, 10, 20},
    {125, 40, 95, 20, 30},
    {75, 40, 100, 30, 40},
    {0, 0, 0, 0, 0} /* terminating condition */
};

float FSB[][2] = { /* Fast-Start-Bonus */
    /* array-values are in format {PV+, %commission} */
    {0, 5},
    {70, 10},
    {120, 15},
    {170, 18},
    {0, 0} /* terminating condition */
};

float TBB[][TBB_MAX_GENERATIONS+1] = { /* Team-Building-Bonus */
    /* array-values are in format {PV+, %1st-gen commission, %2nd-gen commission,...,%7th-gen commission} */
    /* NB: the first generation is the person brought by someone you joined NOT the person you joined! */
    
    /* the terminating condition is TBB_MAX_GENERATIONS for any array(except the last array) */
    
    {0,.5,.5,0,0,0,0,0},
    {70,.5,.5,.5,.5,0,0,0},
    {120,.5,.5,.5,.5,.5,0,0},
    {170,.5,.5,.5,.5,.5,.5,.5},
    {0, 0} /* terminating condition */
};

float TVC[][2] = { /* Team-Volume-Commissions */
    /* ADDITIONAL LOGIC:
       - every leg must have atleast 1 generation ie every person you joined must hve alo joined someone else
       - this is the percentage of the CV of the lowest leg
       - after every calculation, the legs are "cleared" and thus the lowest leg is "reset" to 0
       - you can get commissions of upto your PVin the first month
    */
    /* array-values are in format {PV+, %lesser-leg-commission} */
    {0, 2},
    {70, 3},
    {120, 4},
    {170, 5},
    {0, 0} /* terminating condition */
};

/* all the bonuses below are in %*/
float HOB = .1; /* home-office-bonus */
float LCB = .1; /* laxury-car-bonus */
float EAB = .03; /* expense-account-bonus */
