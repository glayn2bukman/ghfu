"""
    this script should run with cron every day(we cant let it run on the "payment-day" because this can change
    anytime)
    
    the script tells the server to perform monthly operations (auto-refills, TVC and other bonuses that are
    calculated monthly)
"""

import requests

SERVER = "0.0.0.0", 54321

try:
    requests.post("http://{}:{}/monthly_operations", json={"id":"5892cdfWTCSDFEWW8965432"})
except:
    # error occured. is server down? log this issue
    pass
