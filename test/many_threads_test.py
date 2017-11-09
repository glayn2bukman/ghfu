# this script serves to test the multi-threading process handling of libjermGHFU by bombarding
# the server with simultaneous requests (the server in turn calls upon libjermGHFU.so for all its
# operations)

import requests, threading

# MAX-THREADS per process on my system were 30503 ($cat /proc/sys/kernel/threads-max)
THREADS = 1000 # 3000 threads (3 threads per data request to be made)

SERVER = "0.0.0.0", 54321

data = (
    ("register",
        {"uplink":1,"names":"test {}", "deposit":1500}),
    ("buy_package", 
        {"IB_id":1, "amount":1000, "buyer_is_member":True, "buyer_names":"buyer {}"}),
    ("details", 
        {"id":1})
)

for thread in range(THREADS):
    for i in range(len(data)):
        kwargs = data[i][1]
        
        if(i==0): kwargs["names"] = kwargs["names"].format(thread)
        elif(i==1):
            kwargs["buyer_is_member"] = True if thread%2 else False
                
        threading.Thread(
            target=requests.post,
            args=("http://{}:{}/{}".format(SERVER[0],SERVER[1],data[i][0]),),
            kwargs={"json":kwargs}
        ).start()
