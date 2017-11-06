"""
    this script should run with cron every day(we cant let it run on the "payment-day" because this can change
    anytime.). instead, this script invoked the data file and determines the most current payment-day and executes
    intructions according to that informaion!

    when the script runs therefore, it folows simple logic;
    1) load saved data    
    2) check if current day is payment-day. if not so, exit logic
    3) load current structure and call libghfu.perform_monthly_operations with necessary parameters
    4) wait for the next day and repeat
"""

from ctypes import *

