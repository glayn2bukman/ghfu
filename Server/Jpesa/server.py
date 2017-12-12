"""
    This script serves as a simple interface to the jpesa api (see https://secure.jpesa.com)
    The main things to watch out for are;
    
    1) Depositing money from mobile money to jpesa
        - Airtel: the user will always get a notification to enter their PIN to finish the transaction
        - MTN: the user will only get the notification if they have enough balance. 
               therefore, inform the user that if they dont see a prompt in 30s, let them check in thier
               pending approvals and if they still dont see the pending transaction, they dont have enough
               funds on their mobile money account!

        Either way, keep checking the transaction-reference for about 3 minutes (5s intervals maybe?)
        to find out if the transaction was sucessfull.

        Also, inform the user to be with their phone in their hands before they initiate the payment. This
        will reduce on the time taken to make the payment

    2) Redeeming funds from jpesa to mobile money[bank?]
        jpesa requires that this only be done from the jpesa dashboard not the api. as a hack therefore,
        my first suggestion was to try and automate the account creation and funds redeeming process
        so that everytime a person tries to redeem funds, the system logs in into our jpesa account and
        automates the process of liquidating funds to the give mobile money number!

    EXPLORE ALL POSSIBLE RESPONSES FROM THE API AND MAKE A GOOD MODEL OF THE FINANCE MODULE!

    test login credentials are;
        uname: glayn2bukman@gmail.com
        pswd: saltySALTY123*

"""

import sys, requests, json, json, os

jencode = json.JSONEncoder().encode
jdecode = json.JSONDecoder().decode

data_path = "/var/lib/ghfu"

try:
    with open(os.path.join(data_path,".jpesa"),"r") as jpesa_cfg:
        data = jpesa_cfg.read().strip()
        if not (";") in data:
            sys.exit("jpesa file corrupted...")
        separator_index = data.index(";")
        jpesa_uname, jpesa_pswd = data[:separator_index], data[separator_index+1:]
except:
    sys.exit("could not find jpesa configuration file ({})...".format(os.path.join(data_path,".jpesa")))

try:
    with open(os.path.join(data_path,".finance_code"),"r") as finance_code_file:
        finance_server_code = finance_code_file.read().strip()
except: sys.exit("cant find finance server code file ({})".format(os.path.join(data_path,".finance_code")))

jpesa_url = "https://secure.jpesa.com/api.php"

def deposit(number, amount, code):
    "deposit money from mobile money number to jpesa account"
    
    reply = {"status":False, "log":""}

    if code!=finance_server_code:
        reply["log"] = "wrong finance-code given. operation dumped"
        return reply

    if not (isinstance(number, str) or isinstance(number, unicode)):
        reply["log"] = "invalid phone number given. numbers should be strings in format 07********"
        return reply
    
    number = str(number) # just incase its in unicode
    
    if len(number)!=10 or number[:2]!="07":
        reply["log"] = "invalid phone number given. numbers should be in format 07******** with length 10"
        return reply

    if not (isinstance(amount,int) or isinstance(amount,float)):
        reply["log"] = "invalid amount given. expects a numeric value"
        return reply
    
    amount = int(amount)
    
    if amount<1,000 or amount>4950000:
        reply["log"] = "amount must be in the range SHS 1,000-4,950,000"
        return reply
    
    try:
        _reply = requests.post(jpesa_url,
            data = {
                "command":"jpesa",
                "action":"deposit",
                "username":jpesa_uname,
                "password":jpesa_pswd,
                "number":str(number),
                "amount":int(amount)
                }
        ).text
        
        if "ERROR" in _reply:
            reply["log"] = _reply[_reply.index("]")+1:].strip()
        else:
            reply["status"] = True
            reply["ref"] = _reply[_reply.index("|")+1:].strip()
        
    except:
        reply["log"] = "network or url down. if not, you entered an invalid number or amount"

    return reply

def transfer_funds(email, amount, code):
    "deposit money from jpesa account to another jpesa account"
    
    reply = {"status":False, "log":""}

    if code!=finance_server_code:
        reply["log"] = "wrong finance-code given. operation dumped"
        return reply

    if not (isinstance(email, str) or isinstance(email, unicode)):
        reply["log"] = "invalid email given"
        return reply
    
    email = str(email) # just incase its in unicode
    
    if not (isinstance(amount,int) or isinstance(amount,float)):
        reply["log"] = "invalid amount given. expects a numeric value"
        return reply
    
    amount = int(amount)
    
    if amount<1000: # this is the jpesa condition but GHFU has its own that the main server enforces before
                    # is even calls this server
        reply["log"] = "amount must be SHS 1,000+"
        return reply
    
    try:
        _reply = requests.post(jpesa_url,
            data = {
                "command":"jpesa",
                "action":"send",
                "username":jpesa_uname,
                "password":jpesa_pswd,
                "email":email,
                "amount":amount
                }
        ).text
        
        if "ERROR" in _reply:
            reply["log"] = _reply[_reply.index("]")+1:].strip()
        else:
            reply["status"] = True
            reply["ref"] = _reply[_reply.index("|")+1:].strip()
        
    except:
        reply["log"] = "network or url down. if not, you entered an invalid email or amount"

    return reply

def transfer_funds_to_mobile_money(number, amount, code):
    "deposit funds from jpesa account to mobile-money account"
        
    reply = {"status":False, "log":""}

    if code!=finance_server_code:
        reply["log"] = "wrong finance-code given. operation dumped"
        return reply

    if not (isinstance(number, str) or isinstance(number, unicode)):
        reply["log"] = "invalid phone number given. numbers should be strings in format 07********"
        return reply
    
    number = str(number) # just incase its in unicode
    
    if len(number)!=10 or number[:2]!="07":
        reply["log"] = "invalid phone number given. numbers should be in format 07******** with length 10"
        return reply

    if not (isinstance(amount,int) or isinstance(amount,float)):
        reply["log"] = "invalid amount given. expects a numeric value"
        return reply
    
    amount = int(amount)
    
    if amount<1000: # this is the jpesa condition but GHFU has its own that the main server enforces before
                    # is even calls this server
        reply["log"] = "amount must be SHS 1,000+"
        return reply
    
    try:
        _reply = requests.post(jpesa_url,
            data = {
                "command":"jpesa",
                "action":"withdraw",
                "username":jpesa_uname,
                "password":jpesa_pswd,
                "number":number,
                "amount":amount
                }
        ).text
        
        if "ERROR" in _reply:
            reply["log"] = _reply[_reply.index("]")+1:].strip()
        else:
            reply["status"] = True
            reply["ref"] = _reply[_reply.index("|")+1:].strip()
        
    except:
        reply["log"] = "network or url down. if not, you entered an invalid number or amount"

    return reply

# print transfer_funds_to_mobile_money("0701717554", 1500, finance_server_code)

def check_transaction(ref, code):
    "check the status of a transaction"

    reply = {"status":False, "log":""}

    if code!=finance_server_code:
        reply["log"] = "wrong finance-code given. operation dumped"
        return reply

    ref = str(ref)

    try:
        _reply = requests.post(jpesa_url,
            data = {
                "command":"jpesa",
                "action":"info",
                "username":jpesa_uname,
                "password":jpesa_pswd,
                "tid":str(ref)
                }        
        ).text

        if "ERROR" in _reply:
            # this can also happen when there is no matching trnsaction ID, in which case the transaction
            # timeout has most likely passed. ovcourse you'd get this exact error if the ref is wrong
            reply["log"] = _reply[_reply.index("]")+1:].strip()
        else:
            reply["status"] = True
            reply["details"] = jdecode(_reply[_reply.index("]")+1:].strip())
        
    except:
        reply["log"] = "network or url down. if not, you entered an invalid number or amount"

    return reply

# print check_transaction("39DDE5FA2AF0CBA9D1BD15B304333DA1", finance_server_code)
# print check_transaction("54381", finance_server_code)

# actual server code...
from flask import Flask, request, Response

app = Flask(__name__)

# define a function that wil allow responses to be sent to pages not served by this server
def reply_to_remote(reply):
    response = Response(reply)
    response.headers["Access-Control-Allow-Origin"] = "*" # allow all domains...
    return response


@app.route("/test",methods=["GET","POST"])
def test():
    if request.access_route[-1]!="127.0.0.1":
        return reply_to_remote("You are not authorized to perform this operation!"),401
    return reply_to_remote("GHFU finance server is up!")

@app.route("/deposit", methods=["POST"])
def deposit_funds_to_jpesa():
    """
    the function calling this url in the main server should thread it and simply return after calling
    it as this function could hang in a network trying to reach jpesa. 
    
    also, the calling function should return a code to its caller (in the web) so that it can connect
    this code to the transaction reference that will be returned from here. this will simplify the process
    of the web-caller determining if the process was sucessfull 
    """

    if request.access_route[-1]!="127.0.0.1":
        return reply_to_remote("You are not authorized to perform this operation!"),401
    
    reply = {"status":False, "log":""}
    json_req = request.get_json()
    
    if not json_req:
        reply["log"] = "expects json payload!"
        return reply_to_remote(jencode(reply))
    
    number = json_req.get("number","")
    amount = json_req.get("amount",0)
    code = json_req.get("code","")
    
    reply = deposit(number,amount,code)
    
    return reply_to_remote(jencode(reply))

@app.route("/transfer_funds", methods=["POST"])
def transfer_funds_to_jpesa():
    """
    the function calling this url in the main server should thread it and simply return after calling
    it as this function could hang in a network trying to reach jpesa. 
    
    also, the calling function should return a code to its caller (in the web) so that it can connect
    this code to the transaction reference that will be returned from here. this will simplify the process
    of the web-caller determining if the process was sucessfull 
    """

    if request.access_route[-1]!="127.0.0.1":
        return reply_to_remote("You are not authorized to perform this operation!"),401
    
    reply = {"status":False, "log":""}
    json_req = request.get_json()
    
    if not json_req:
        reply["log"] = "expects json payload!"
        return reply_to_remote(jencode(reply))
    
    email = json_req.get("email","")
    amount = json_req.get("amount",0)
    code = json_req.get("code","")
    
    reply = transfer_funds(email,amount,code)
    
    return reply_to_remote(jencode(reply))

@app.route("/transfer_funds_to_mobile_money", methods=["POST"])
def transfer_funds_from_jpesa_to_mobile_money():
    """
    the function calling this url in the main server should thread it and simply return after calling
    it as this function could hang in a network trying to reach jpesa. 
    
    also, the calling function should return a code to its caller (in the web) so that it can connect
    this code to the transaction reference that will be returned from here. this will simplify the process
    of the web-caller determining if the process was sucessfull 
    """

    if request.access_route[-1]!="127.0.0.1":
        return reply_to_remote("You are not authorized to perform this operation!"),401
    
    reply = {"status":False, "log":""}
    json_req = request.get_json()
    
    if not json_req:
        reply["log"] = "expects json payload!"
        return reply_to_remote(jencode(reply))
    
    number = json_req.get("number","")
    amount = json_req.get("amount",0)
    code = json_req.get("code","")
    
    reply = transfer_funds_to_mobile_money(number,amount,code)
    
    return reply_to_remote(jencode(reply))


@app.route("/transaction_status", methods=["POST"])
def check_jpesa_transaction_status():
    """
    unlike /deposit above, this on should not branch off in the calling function in the main server.
    rather, the main server function calling this url should block and actually wait for the message
    coming from the jpesa api regarding the transaction in question 
    """

    if request.access_route[-1]!="127.0.0.1":
        return reply_to_remote("You are not authorized to perform this operation!"),401
    
    reply = {"status":False, "log":""}
    json_req = request.get_json()
    
    if not json_req:
        reply["log"] = "expects json payload!"
        return reply_to_remote(jencode(reply))
    
    ref = json_req.get("ref","")
    code = json_req.get("code","")

    reply = check_transaction(ref, code)

    return reply_to_remote(jencode(reply))

#
#
# strictly for martin's development
#
#
import random
@app.route("/test_transaction_status", methods=["POST"])
def test_check_jpesa_transaction_status():
    """
    unlike /deposit above, this on should not branch off in the calling function in the main server.
    rather, the main server function calling this url should block and actually wait for the message
    coming from the jpesa api regarding the transaction in question 
    """

    if request.access_route[-1]!="127.0.0.1":
        return reply_to_remote("You are not authorized to perform this operation!"),401
    
    reply = {"status":False, "log":""}
    json_req = request.get_json()
    
    if not json_req:
        reply["log"] = "expects json payload!"
        return reply_to_remote(jencode(reply))
    
    ref = json_req.get("ref","")
    code = json_req.get("code","")

    rep = random.randint(0,10)
    r = random.randint(0,10)
    #if rep<5:
    #    reply["details"] = {"status":"pending", "tid":"{rep}A{r}E{rep}FT05TDG".format(rep=rep, r=r)}
    #if rep%2:
    #    reply["log"] = "No Record Matching Transaction ID"
    #else:
    reply["status"] = True
    reply["details"] = {"status":"complete", "tid":"{rep}A{r}E{rep}FT05TDG".format(rep=rep, r=r)}

    return reply_to_remote(jencode(reply))

@app.route("/test_deposit", methods=["POST"])
def test_deposit_funds_to_jpesa():
    """
    the function calling this url in the main server should thread it and simply return after calling
    it as this function could hang in a network trying to reach jpesa. 
    
    also, the calling function should return a code to its caller (in the web) so that it can connect
    this code to the transaction reference that will be returned from here. this will simplify the process
    of the web-caller determining if the process was sucessfull 
    """

    if request.access_route[-1]!="127.0.0.1":
        return reply_to_remote("You are not authorized to perform this operation!"),401
    
    reply = {"status":False, "log":""}
    json_req = request.get_json()
    
    if not json_req:
        reply["log"] = "expects json payload!"
        return reply_to_remote(jencode(reply))
    
    number = json_req.get("number","")
    amount = json_req.get("amount",0)
    code = json_req.get("code","")
    
    if code!=finance_server_code:
        reply["log"] = "wrong finance-code given. operation dumped"
        return reply_to_remote(jencode(reply))

    if not (isinstance(number, str) or isinstance(number, unicode)):
        reply["log"] = "invalid phone number given. numbers should be strings in format 07********"
        return reply_to_remote(jencode(reply))
    
    number = str(number) # just incase its in unicode
    
    if len(number)!=10 or number[:2]!="07":
        reply["log"] = "invalid phone number given. numbers should be in format 07******** with length 10"
        return reply_to_remote(jencode(reply))

    if not (isinstance(amount,int) or isinstance(amount,float)):
        reply["log"] = "invalid amount given. expects a numeric value"
        return reply_to_remote(jencode(reply))
    
    amount = int(amount)
    
    if amount<500 or amount>4950000:
        reply["log"] = "amount must be in the range SHS 1,000-4,950,000"
        return reply_to_remote(jencode(reply))

    rep = random.randint(0,10)
    r = random.randint(0,10)
    #if rep%2:
    #    reply["log"] = "SIMULATED-ERROR"
    #else:
    reply["status"] = True
    reply["ref"] = "{rep}A{r}E{rep}FT05TDG".format(rep=rep, r=r)
    
    
    return reply_to_remote(jencode(reply))

@app.route("/test_transfer_funds_to_mobile_money", methods=["POST"])
def test_transfer_funds_from_jpesa_to_mobile_money():
    """
    the function calling this url in the main server should thread it and simply return after calling
    it as this function could hang in a network trying to reach jpesa. 
    
    also, the calling function should return a code to its caller (in the web) so that it can connect
    this code to the transaction reference that will be returned from here. this will simplify the process
    of the web-caller determining if the process was sucessfull 
    """

    if request.access_route[-1]!="127.0.0.1":
        return reply_to_remote("You are not authorized to perform this operation!"),401
    
    reply = {"status":False, "log":""}
    json_req = request.get_json()
    
    if not json_req:
        reply["log"] = "expects json payload!"
        return reply_to_remote(jencode(reply))
    
    number = json_req.get("number","")
    amount = json_req.get("amount",0)
    code = json_req.get("code","")
    
    if code!=finance_server_code:
        reply["log"] = "wrong finance-code given. operation dumped"
        return reply_to_remote(jencode(reply))

    if not (isinstance(number, str) or isinstance(number, unicode)):
        reply["log"] = "invalid phone number given. numbers should be strings in format 07********"
        return reply_to_remote(jencode(reply))
    
    number = str(number) # just incase its in unicode
    
    if len(number)!=10 or number[:2]!="07":
        reply["log"] = "invalid phone number given. numbers should be in format 07******** with length 10"
        return reply_to_remote(jencode(reply))

    if not (isinstance(amount,int) or isinstance(amount,float)):
        reply["log"] = "invalid amount given. expects a numeric value"
        return reply_to_remote(jencode(reply))
    
    amount = int(amount)
    
    if amount<1000: # this is the jpesa condition but GHFU has its own that the main server enforces before
                    # is even calls this server
        reply["log"] = "amount must be SHS 1,000+"
        return reply_to_remote(jencode(reply))

    rep = random.randint(0,10)
    r = random.randint(0,10)
    #if rep%2:
    #    reply["log"] = "SIMULATED-ERROR"
    #else:
    reply["status"] = True
    reply["ref"] = "{rep}A{r}E{rep}FT05TDG".format(rep=rep, r=r)
    
    return reply_to_remote(jencode(reply))


if __name__=="__main__":
    app.run("0.0.0.0", 54322, threaded=1, debug=1)
