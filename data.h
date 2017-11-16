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

/* account fees($) */
float ACCOUNT_CREATION_FEE = 40.0; /* $ */
float ANNUAL_SUBSCRIPTION_FEE = 10.0; /* $ */
float OPERATIONS_FEE = 180.0; /* $ */
float MINIMUM_INVESTMENT = 500.0; /* 125 points */
float MAXIMUM_INVESTMENT = 1500.0; /* 375 points */


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
    "Diamond"
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
    /*9*/ "Presidential Director",
    /*10*/ "Senior Presidential Director",
    /*11*/ "Crown",
    /*12*/ "\0", /* termination condition*/
};

float RANK_DETAILS[][4] = {
    /* volumes required to reach a certain rank. order;
        higest rank in any of the legs, PV, cummulative organisation volume, award
         
    */
    /*0*/  {0,  0,    0,        0},
    /*1*/  {0,  100,  1000,     0},
    /*2*/  {0,  100,  3000,     0},
    /*3*/  {0,  100,  10000,    0},
    /*4*/  {0,  100,  30000,    0},
    /*5*/  {0,  200,  100000,   500},
    /*6*/  {0,  200,  300000,   1000},
    /*7*/  {0,  200,  1000000,  2500},
    /*8*/  {7,  0,    0,        6000},
    /*9*/  {8,  0,    0,        15000},
    /*10*/ {9,  0,    0,        25000},
    /*11*/ {10, 0,    0,        50000},
    
    /*12*/ {0,  1,    1,        0} /* terminating condition */
};

/* commission/bonus ranges (all amounts in points NOT $)*/

float IBC[][2] = { /* independent-brocker=commissions */
    /* array-values are in format {PV+, %commision} */
    {0, 35},
    {120, 40},
    {200, 45},
    {350, 50},
    {0, 0} /* terminating condition */
};

float INVESTMEN_SCHEME[][3] = {
    /* array-values are in format {points+, %lowest, %highest} 
       the data is in descending rank(points) order
    */
    {375, 40, 80},
    {250, 40, 70},
    {125, 40, 65},
    {0, 0, 0} /* terminating condition */
};

float FSB[][2] = { /* Fast-Start-Bonus */
    /* array-values are in format {PV+, %commission} */
    {0, 3},
    {120, 4},
    {250, 6},
    {350, 8},
    {0, 0} /* terminating condition */
};

float TBB[][TBB_MAX_GENERATIONS+1] = { /* Team-Building-Bonus */
    /* array-values are in format {PV+, %1st-gen commission, %2nd-gen commission,...,%7th-gen commission} */
    /* NB: the first generation is the person brought by someone you joined NOT the person you joined! */
    
    /* the terminating condition is TBB_MAX_GENERATIONS for any array(except the last array) */
    
    {0,.5,.5,0,0,0,0,0},
    {120,.5,.5,.5,.5,0,0,0},
    {200,.5,.5,.5,.5,.5,0,0},
    {350,.5,.5,.5,.5,.5,.5,.5},
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
    {120, 3},
    {200, 4},
    {350, 5},
    {0, 0} /* terminating condition */
};

typedef struct director 
{
    char *title;
    float award;
} Director;

Director DRCA[] = { /* DIRECTOR'S RECOGNITION CASH AWARD */
    {"Director", 500},
    {"Senior Director", 1000},
    {"Executive Director", 2500},
    {"Senior Executive Director", 6000},
    {"Presidential Director", 15000},
    {"Senior Presidential Director", 25000},
    {"Crown", 50000},
    {"END", 0}, /* terminating condition */
};

/* all the bonuses below are in %*/
float HOB = .05; /* home-office-bonus */
float LCB = .05; /* laxury-car-bonus */
float EAB = .03; /* expense-account-bonus */
