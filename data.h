/* the conversion factor from amount($) to points */
#define POINT_FACTOR 0.25
#define TBB_MAX_GENERATIONS 7
#define PAYMENT_DAY 26 /* any day from 1'st to 28'th (29+ may be absent in some months...) */
#define TVC_DAY 26 /* any day from 1'st to 28'th (29+ may be absent in some months...) */

/* account fees($) */
#define ACCOUNT_CREATION_FEE 40 /* $ */
#define ANNUAL_SUBSCRIPTION_FEE 10 /* $ */
#define OPERATIONS_FEE 180 /* $ */
#define MINIMUM_INVESTMENT 500 /* 125 points */
#define MAXIMUM_INVESTMENT 1500 /* 375 points */

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

/* account types */
char *ACCOUNTS[] = {
    "Independent Brocker",
    "Saphire",
    "Ruby",
    "Diamond"
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
    /* array-values are in format {PV+, %1st-gen commission, %2nd-gen commission,...,%9th-gen commission} */
    /* NB: the first generation is the person brought by someone you joined NOT the person you joined! */
    
    /* the last value in each array is 0(terminating condition) */
    
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

float HOB = .05; /* home-office-bonus */
float LCB = .05; /* laxury-car-bonus */
float EAB = .03; /* expense-account-bonus */
