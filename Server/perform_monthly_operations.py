"""
    this script should run everyday(we cant let it run only on the "payment-day" because this can 
    change anytime)
    
    the script tells the server to perform monthly operations (auto-refills, TVC and other bonuses that are
    calculated monthly)
"""

import requests, os, time

SERVER = "0.0.0.0", 54321
data_path = "/var/lib/ghfu"
sleep_time = 24*3600

while 1:
    try:
        with open(os.path.join(data_path, ".monthly_operations_code"),"r") as f:
            code = f.read().strip()
        requests.post("http://{}:{}/monthly_operations".format(*SERVER), json={"id":code})
    except:
        # error occured. is server down? log this issue
        pass

    time.sleep(sleep_time)
