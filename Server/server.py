"""
    ==== ABOUT ====    
    this script acts as the python web-API between libjermGHFU.so and martin's web front-end for GHFU.
    libjermGHFU.so is thread-safe so no worries(?) about multiple threads(incoming requests) updating the same
    data at the same time
    
    ==== MARTIN'S WARNING ====
    no account sould be logged in more than once at any one time. this will greatly reduce on the 
    thread-safe labor libjermGHFU.so has. infact, if one account is logged on twice simultaneously, this
    will fuck up every thing in terms of server log responses as (look at how the logfile is defines in 
    every function to understand this)
    
    ==== RUNNING & DEBUGGING ====     
    the server program, in debugging mode, Ctrl-C/Z might not stop the program completely. in that case, 
    you'll get a "port already taken" error when you attempt to re-run the server. if that happens, run;
        $ netstat -tulpn | grep $(PORT=54321; echo $PORT) # set PORT to whatever port is being used
    and when you get the PID (in the last column ie PID/python), kill the process with
        $ kill -9 PID
    
    in deployment, run this with twisted;
        $ twistd web --wsgi Server.server # app=server, module=Server

"""
# the most important interfaces to the dynamic library (libjermGHFU.so) are;
#   ID account_id(Account account);
#   bool dump_structure_details(ID account_id, String fout_name); 
#   Account get_account_by_id(const ID id);
#   ID account_id(Account account);
#   bool invest(ID account_id, const Amount amount, const String package, 
#        const ID package_id, const bool update_system_float, String fout_name);
#   void perform_monthly_operations(float auto_refill_percentages[4][2], String fout_name);
#   void purchase_property(ID IB_account_id, const Amount amount, const bool member, 
#        const String buyer_names, String fout_name);
#   bool redeem_account_points(ID account_id, Amount amount, String fout_path);
#   ID register_new_member(ID uplink_id, String names, Amount amount, String fout_name);

#   NB: String is defined as (char *) in ghfu.c. bool is defined as (enum bool {false, true}),
#       ID is defined as (unsigned long)

# ALL FUNCTIONS IN THIS SCRIPT SHOULD ACCEPT BOTH FORMS AND JSON DATA AS REQUEST(FOR FLEXIBILITY) BUT WILL 
# ALWAYS RETURN JSON

from flask import Flask, request, Response
import os, sys, json, threading, time, random
import requests

path = os.path.realpath(__file__)
path = os.path.split(os.path.split(path)[0])[0]

data_path = "/var/lib/ghfu"

try:
    with open(os.path.join(data_path,".finance_code"),"r") as finance_code_file:
        finance_server_code = finance_code_file.read().strip()
except: sys.exit("cant find finance server code file ({})".format(os.path.join(data_path,".finance_code")))

from ctypes import *


libghfu = CDLL(os.path.join(path,"lib","libjermGHFU.so"))

# define libghfu function argtypes (so that we can call them normally and let ctypes do any type conversions)
libghfu.invest.argtypes = [c_long, c_float, c_char_p, c_long, c_int, c_char_p]
libghfu.dump_structure_details.argtype = [c_long, c_char_p]
libghfu.get_account_by_id.argtypes = [c_long]
#libghfu.perform_monthly_operations.argtypes = [(c_float*2)*4, c_char_p]
libghfu.purchase_property.argtypes = [c_long, c_float, c_char_p]
libghfu.redeem_account_points.argtypes = [c_long, c_float, c_char_p]
libghfu.register_new_member.argtypes = [c_long, c_char_p, c_float, c_char_p]
libghfu.set_constant.argtypes = [c_char_p, c_float]
libghfu.save_structure.argtypes = [c_char_p, c_char_p]

libghfu.account_id.restype = c_long

# libghfu.perform_monthly_operations is called as follows
#   data = [(375,64),(250,60),(125,49),(0,0)]
#   libghfu.perform_monthly_operations(((c_float*2)*4)(*data), "fout")

#   NB data is a LIST of TUPLES. and the last TUPLE MUST BE (0,0) as this is the terminating 
#      condition in libghfu

server_log_file_path = os.path.join(path,"Server","log")
server_log_file = open(server_log_file_path,"a")

def server_log(_log):
    server_log_file.write("\n{}: {}".format(time.asctime(), _log))

app = Flask(__name__)

known_clients = "127.0.0.1"

jencode = json.JSONEncoder().encode
jdecode = json.JSONDecoder().decode

#mutex = threading.Lock()

LAST_PERFORMED_MONTHLY_OPERATIONS = [0,0,0]

EXCHANGE_RATE=3600 # deposit rate is always 100/= more DICUSS THIS WITH THE GHFU ADMINS. withdraw value
                   # is always 50/= less
                   # also, this value can be modified anytime using uri </update_exchange_rate> but once
                   # every half-day, the server will first attempt to collect the latest value at
                   # <http://usd.fxexchangerate.com/ugx/> and in case it fails, then the manually set
                   # value will be used!

TRANSACTION_CHECK_DELAY = 30 # delay for checking if pending transaction has been effected
JPESA_DEPOSIT_CHARGES = .03

CODES = {} # every time a random code is generated, its stored here to hold transaction data
           # so when the client wants to know about the state of a transaction, they i dont querry the 
           # jpesa api but rather, i cnosult with this dictionary and return whatever the latest status
           # of the transaction is kept here

# remove used uncessesary file
def rm(f):
    try:os.remove(f)
    except:pass

# modify file path
def file_path(fname):
    return os.path.join(path, "files","out",fname)

# function to fetch logs/errors
def info(fname):
    try:
        with open(file_path(fname),"r") as f: 
            return f.read()
    except:
        return "failed to access json file. is account signed in multiple times"

# define a function that wil allow responses to be sent to pages not served by this server
def reply_to_remote(reply):
    response = Response(reply)
    response.headers["Access-Control-Allow-Origin"] = "*" # allow all domains...
    return response

# define scanning function (we ony wanna accept requests from known clients)
def client_known(addr):
    if not(os.path.isfile(os.path.join(data_path,".known_clients"))):
        with open(os.path.join(data_path,".known_clients"), "w") as f: f.write(known_clients)
    with open(os.path.join(data_path,".known_clients"), "r") as f:
        known = [c.strip() for c in f.read().strip().split("\n")]
        for client in known:
            if addr==client: return True
    return False

# define function to auto-update the exchange rate every half-day
def fetch_current_exchange_rate():
    global EXCHANGE_RATE
    exchange_rate_url = "http://usd.fxexchangerate.com/ugx/"
    
    while 1:
        try:
            data = requests.get(exchange_rate_url).text
            try:
                i = data.index(" UGX")
                if i!=13956:
                    server_log("the index of \" UGX\" in the exchange-rates \
url aint where we expected it to be,\
none-the-less, continuing with the operation")
                
                value = data[i-5:i] # very likely that (1,000 <= USD <= 10,000)
                try:
                    value = int(value)
                except:
                    server_log("the exchange-rates site must have changed configuration, \
no data could be read using our algorithm")
                                
                if value<1000: # very likely that (1,000 <= USD <= 10,000)
                    server_log("got <{}> as the exchange-rate value from the exchange-rates url.\
however, this value just is suspicious and we aint gonna use it!".format(value))
                else:
                    EXCHANGE_RATE = value
                    
            except:
                server_log("is the exchange-rates url right? cnt find the index of \" UGX\" in it")
        except: 
            server_log("the exchange-rates url <{}> was moved".format(exchange_rate_url))
        
        time.sleep(12*60*60)

# define function to keep checking on transaction deposit statuses and effect changes to ghfu if the 
# transaction(deposit/withdraw) is verified. if so, the necesary update to GHFU is effected
def effect_transaction(code, reference, func=None, args=None, logfile=None):
    "amount is in dollars ie the amount to feed to jermlibghfu.so"
    
    while 1:
        try:
            reply = requests.post("http://0.0.0.0:{}/transaction_status".format(finance_server_port), 
                json={"code":finance_server_code, "ref":reference}).text
            reply = jdecode(reply)
            if not reply["status"]:
                CODES[code]["status"] = False
                CODES[code]["actionlog"] = "Operation bounced."
                break
        except: # finance server is down for some reason...
            server_log("the finance server is down. look into this ASAP!")
            time.sleep(TRANSACTION_CHECK_DELAY)
            continue
        
        if reply["status"] and reply["details"]["status"]=="complete":
            if not logfile:
                logfile = file_path("{}.transaction".format(code))

            func(*args)
            
            if info(logfile): 
                CODES[code]["status"] = False
                CODES[code]["actionlog"] = info(logfile)
            else:
                CODES[code]["status"] = True
                CODES[code]["details"] = reply
                CODES[code]["actionlog"] = ""
                                        
            rm(logfile)
            
            break
        
        time.sleep(TRANSACTION_CHECK_DELAY)

# function to initialte deposit trancaction to jpesa
def depost_funds_to_jpesa(internal_code, number, amount, func, args):
    """
    this function should be brached off from the callee ie threaded

    typically, we call the function in the following steps;

    ``
    new_internal_code = get_random_code()
    CODES[new_internal_code] = {"status":False, "actionlog":"pending"}
    
    threading.Thread(target=depost_funds_to_jpesa, args=(new_internal_code,"0701173049", 500)).start()
    
    reply["status"] = True
    reply["code"] = new_internal_code
    ``

    """
    try:
        reply = requests.post("http://0.0.0.0:{}/deposit".format(finance_server_port), 
                json={"code":finance_server_code, "number":number, "amount":amount}).text
        reply = jdecode(reply)
        if not reply["status"]:
            CODES[code]["status"] = False
            CODES[code]["actionlog"] = "An error occured in the financial server..."
        else:
            effect_transaction(internal_code,reply["ref"],func, args)
    except:
        CODES[internal_code]["status"] = False
        CODES[internal_code]["actionlog"] = "Failed to reach finance server"


# function to generate rado codes
def get_random_code():
    "generate and return a random code 8 xters long"
    t = str(time.time())
    t = t[t.index(".")+1:]
    
    if len(t)>=8:
        t = t[:8]
        return t
    
    for i in range(8-len(t)):
        t += str(random.randint(0,9))

    return t


@app.route("/test",methods=["GET","POST"])
def test():

    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401
    reply = {"status":"GHFU server is up!"}
    
    return reply_to_remote(jencode(reply))
    

@app.route("/register",methods=["POST"])
def register():
    """ if returned json has <id> set to 0, check for the log from key <log>"""


    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"id":0, "log":""}

    json_req = request.get_json()

    # deposit should include registration,anual server charges + operations fee if investing

    if json_req:
        uplink_id = json_req.get("uplink",-1)
        names = str(json_req.get("names",-1))
        deposit = json_req.get("deposit",-1)

    else:
        uplink_id = request.form.get("uplink",-1)
        names = request.form.get("names",-1)
        deposit = request.form.get("deposit",-1) 

        try:
            uplink_id = int(uplink_id)
            deposit = float(deposit)
        except:
            reply["log"] = "silly form data provided for uplink(id) or deposit"
            return reply_to_remote(jencode(reply))

    if (uplink_id==-1 or type(uplink_id)!=type(0)):
        reply["log"] = "silly data provided; parameter <uplink_id>"
        return reply_to_remote(jencode(reply))
    if not names:
        reply["log"] = "silly data provided; parameter <names>"
        return reply_to_remote(jencode(reply))
    if (deposit<0 or isinstance(deposit,unicode) or (not (type(deposit)!=type(0) or type(deposit)!=type(0.0)))):
        reply["log"] = "silly data provided; parameter <deposit>"
        return reply_to_remote(jencode(reply))
    
    # allow creating accounts with ROOT uplinks? i doubt 
    if uplink_id==0:
        reply["log"] = "you dont have the permission to add accounts to ROOT!"
        return reply_to_remote(jencode(reply))
    else:
        logfile = file_path("{}{}{}.reg".format(uplink_id,names,deposit))

        account_id = libghfu.register_new_member(uplink_id, names, deposit, logfile)
        if account_id: 
            reply["id"]=account_id

            threading.Thread(target=libghfu.save_structure, args=(
                os.path.join(path,"lib"), os.path.join(path,"files","saves")
                )).start()
        else: 
            reply["log"] = info(logfile)
    
        rm(logfile)
        
    return reply_to_remote(jencode(reply))

@app.route("/details", methods=["POST"])
def details():
    """ if returned json has <id> set to 0, check for the <log>. otherwise, deal with json data as per the
    standard;
    
        {
            "names": str,
            "id": int/long,
            "uplink": str,       /* the uplink's name NOT id, we dnt wanna compromise on security here */
            "pv": float,
            "total_returns": float,
            "available_balance": float,
            "total_redeems": float,
            "commissions": [[float amount, str reason],...],
            "leg_volumes": [float leg-1-volume, float leg-2-volume, float leg-3-volume],
            "investments": [[str date, float points, str package, int package id, int months_returned, returns],...],
            "direct_children":[str child1_names, str child2_names,.....] /* names NOT ids, again, its for 
                                                                            for security reasons */
        }
    
    """

    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"id":0, "log":""}
    
    json_req = request.get_json()

    if json_req:
        account_id = json_req.get("id",-1)
    else:
        account_id = request.form.get("id",-1)

        try:
            account_id = int(account_id)
        except:
            reply["log"] = "silly form data provided; parameter <id>"
            return reply_to_remote(jencode(reply))
    
    if(account_id==-1 or type(account_id)!=type(0) or isinstance(account_id,unicode) ):
        reply["log"] = "silly data provided; parameter <id>"
        return reply_to_remote(jencode(reply))
    
    jsonfile = os.path.join(path, "files","json","{}.json".format(account_id))
        
    if libghfu.dump_structure_details(account_id, jsonfile):
        try:
            with open(jsonfile, "r") as f: reply = f.read()
            rm(jsonfile)
        except:
            reply["log"] = "failed to access json file. is account signed in multiple times"
    else:
        reply["log"] = "no account matching requested target!"
        reply = jencode(reply)
    
    return reply_to_remote(reply)

@app.route("/buy_package", methods=["POST"])
def buy_package():
    " if returned json <status> key is true, all went well, otherwise, check <log>"

    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()
    
    if json_req:
        IB_id = json_req.get("IB_id",-1)
        amount = json_req.get("amount",0)

    else:
        IB_id = request.form.get("IB_id",-1)
        amount = request.form.get("amount",-1)

        try:
            IB_id = int(IB_id)
            amount = float(amount)
        except:
            reply["log"] = "silly form data provided"
            return reply_to_remote(jencode(reply))

    if(IB_id==-1 or type(IB_id)!=type(0) or isinstance(IB_id,unicode) ):
        reply["log"] = "silly data provided; parameter <IB_id>"
        return reply_to_remote(jencode(reply))
    if(amount==0 or (not (type(amount)==type(0.0) or type(amount)==type(0))) or isinstance(amount,unicode) ):
        reply["log"] = "silly data provided; parameter <amount>"
        return reply_to_remote(jencode(reply))

    logfile = file_path("{}{}.buy_pkg".format(IB_id,amount))

    if libghfu.purchase_property(IB_id, c_float(amount), logfile):
        reply["status"]=True

        threading.Thread(target=libghfu.save_structure, args=(
            os.path.join(path,"lib"), os.path.join(path,"files","saves")
            )).start()
    else:
        reply["log"] = info(logfile)

    rm(logfile)

    return reply_to_remote(jencode(reply))

@app.route("/data_constants", methods=["POST"])
def get_data_constants():
    """ return data constants ie;
        POINT_FACTOR,PAYMENT_DAY,ACCOUNT_CREATION_FEE,ANNUAL_SUBSCRIPTION_FEE,
        OPERATIONS_FEE,MINIMUM_INVESTMENT,MAXIMUM_INVESTMENT
    """

    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {}

    reply["point-factor"] = c_float.in_dll(libghfu, "POINT_FACTOR").value
    reply["payment-day"] = c_int.in_dll(libghfu, "PAYMENT_DAY").value
    reply["account-creation-fee"] = c_float.in_dll(libghfu, "ACCOUNT_CREATION_FEE").value
    reply["annual-subscription-fee"] = c_float.in_dll(libghfu, "ANNUAL_SUBSCRIPTION_FEE").value
    reply["operations-fee"] = c_float.in_dll(libghfu, "OPERATIONS_FEE").value
    reply["minimum-investment"] = c_float.in_dll(libghfu, "MINIMUM_INVESTMENT").value
    reply["maximum-investment"] = c_float.in_dll(libghfu, "MAXIMUM_INVESTMENT").value

    return reply_to_remote(jencode(reply))

@app.route("/set_data_constants", methods=["POST"])
def set_data_constants():
    """ set data constants ie;
        POINT_FACTOR,PAYMENT_DAY,ACCOUNT_CREATION_FEE,ANNUAL_SUBSCRIPTION_FEE,
        OPERATIONS_FEE,MINIMUM_INVESTMENT,MAXIMUM_INVESTMENT
    """

    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {}

    json_req = request.get_json()

    # libghfu.set_constant expects a FLOAT. giving it an INT will destroy saved structure files n all hell
    # will break loose

    if json_req:
        for key in json_req:
            if(isinstance(json_req[key],int) or isinstance(json_req[key],float)):
                reply[key] = libghfu.set_constant(key, float(json_req[key]))
                reply[key] = True if reply[key] else False
            else: reply[key]=False
    else:
        for key in request.form:
            try:
                value = float(request.form[key])
                reply[key] = libghfu.set_constant(key, value)
                reply[key] = True if reply[key] else False
            except: reply[key]=False

    for key in reply:
        if reply[key]:

            threading.Thread(target=libghfu.dump_constants, args=(
                os.path.join(path,"lib"), os.path.join(path,"files","saves")
                )).start()

            break

    return reply_to_remote(jencode(reply))


@app.route("/invest", methods=["POST"])
def invest():
    " if returned json <status> key is true, all went well, otherwise, check <log>"

    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()
    
    if json_req:
        account_id = json_req.get("id", -1)
        amount = json_req.get("amount", -1)
        package = str(json_req.get("package", ""))
        package_id = json_req.get("package_id", -1)

    else:
        account_id = request.form.get("id",-1)
        amount = request.form.get("amount", -1)
        package = request.form.get("package", "")
        package_id = request.form.get("package_id", -1)

        try:
            account_id = int(account_id)
            amount = float(amount)
            package_id = int(package_id)
        except:
            reply["log"] = "silly form data provided"
            return reply_to_remote(jencode(reply))
            
    if(account_id==-1 or type(account_id)!=type(0) or isinstance(account_id,unicode) ):
        reply["log"] = "silly data provided; parameter <account_id>"
        return reply_to_remote(jencode(reply))
    if(amount==-1 or (not(type(amount)!=type(0) or type(amount)!=type(0.0))) or isinstance(amount,unicode) ):
        reply["log"] = "silly data provided; parameter <amount>"
        return reply_to_remote(jencode(reply))
    if not package:
        reply["log"] = "silly data provided; parameter <package>"
        return reply_to_remote(jencode(reply))
    if(package_id==-1 or type(package_id)!=type(0) or isinstance(package_id,unicode) ):
        reply["log"] = "silly data provided; parameter <package_id>"
        return reply_to_remote(jencode(reply))

    logfile = file_path("{}{}{}{}.inv".format(account_id,amount,package,package_id))

    package = str(package)
    
    if libghfu.invest(account_id, amount, package, package_id, 1, logfile):
        reply["status"]=True

        threading.Thread(target=libghfu.save_structure, args=(
            os.path.join(path,"lib"), os.path.join(path,"files","saves")
            )).start()
    else:
        reply["log"] = info(logfile)

    rm(logfile)

    return reply_to_remote(jencode(reply))

@app.route("/auto-refills", methods=["POST"])
def update_auto_refills():
    """
     this task should be done monthly a day or two before payment day. infact martin's system should reming the 
     necessary people about this atleast 2 days prior to payment!
    """

    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()
    
    if not json_req:
        reply["log"] = "server expects json here"
        return reply_to_remote(jencode(reply))

    data = json_req.get("data", [])

    if not data:
        reply["log"] = "no data found in request (key: data, value:array of 2-item-arrays)"
        return reply_to_remote(jencode(reply))
    
    if not isinstance(data, list):
        reply["log"] = "invalid data found in request. server expects array of 2-item-arrays as value of \"data\""
        return reply_to_remote(jencode(reply))

    for d in data:
        if not isinstance(d, list):
            reply["log"] = "malformed data found in request only 2-item arrays expected in array value of key data"
            return reply_to_remote(jencode(reply))
        if len(d)!=2:
            reply["log"] = "malformed data found in request only 2-item arrays expected in array value of key data"
            return reply_to_remote(jencode(reply))
        for i in d:
            if not (isinstance(i, int) or isinstance(i, float)):
                reply["log"] = "malformed data found in request only 2-item arrays expected in array value of key data"
                return reply_to_remote(jencode(reply))
    
    # ALWAYS in DESCENDING ORDER OF pv
    # the last item is ALWAYS (0,0). this is the termination condition in libjermGHFU.so
    data.sort()
    data.reverse()
    data.append([0,0])
    data = [tuple(pair) for pair in data]

    floats = (c_float*2)*len(data)

    reply["status"] = libghfu.update_monthly_auto_refill_percentages(
        floats(*data),
        os.path.join(path,"lib"),
        os.path.join(path,"files","saves")
    )
    
    reply["status"] = True if reply["status"] else False

    return reply_to_remote(jencode(reply))
    

@app.route("/monthly_operations", methods=["POST"])
def perform_monthly_operations():
    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    if request.access_route[-1]!="127.0.0.1":
        # the only authorized script to call this is local to the system (infact, its in the same directory
        # as this server script!)
        return reply_to_remote(jencode({"status":False, 
            "log":"You are not authorised to perform this operation!"}))

    json_req = request.get_json()
    
    if not json_req:
        return reply_to_remote("You are not authorised to perform this operation!"),401
        
    ID = json_req.get("id", "")

    if (not ID) or ID!="5892cdfWTCSDFEWW8965432":
        return reply_to_remote("You are not authorised to perform this operation!"),401

    pd = c_int.in_dll(libghfu, "PAYMENT_DAY").value

    t = time.gmtime()
    today = [t[0], t[1], t[2]]
    
    if pd!= today[2]:
        return reply_to_remote(jencode({"status":False, "log":"not payment day"}))
    
    global LAST_PERFORMED_MONTHLY_OPERATIONS
    
    if today==LAST_PERFORMED_MONTHLY_OPERATIONS:
        return reply_to_remote(jencode({"status":False, "log":"operation already performed"}))

    LAST_PERFORMED_MONTHLY_OPERATIONS = today[:]

    # the auto-refill percentages should have been set atleast 2 days ago so we dont specify any
    # percentages to libghfu.perform_monthly_operations so that it can use the latest values

    logfile = file_path("mo.ghfu")

    if libghfu.perform_monthly_operations(logfile):
        rm(logfile)

        threading.Thread(target=libghfu.save_structure, args=(
            os.path.join(path,"lib"), os.path.join(path,"files","saves")
            )).start()

        return reply_to_remote(jencode({"status":True, "log":""}))

    log = info(logfile)
    rm(logfile)

    return reply_to_remote(jencode({"status":False, "log":log}))

@app.route("/get_exchange_rate", methods=["POST"])
def get_current_exchange_rate():
    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    return reply_to_remote(jencode({"value":EXCHANGE_RATE}))

@app.route("/update_exchange_rate", methods=["POST"])
def update_exchange_rate():
    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()
    
    if not json_req:
        reply["log"] = "server expects json here"
        return reply_to_remote(jencode(reply))

    er = json_req.get("rate",0) # dollar->ugx rate eg 3600

    try: er = float(er)
    except:
        reply["log"] = "invalid rate given. expecting float or integer"
        return reply_to_remote(jencode(reply))

    EXCHANGE_RATE = er

    reply["status"] = True

    return reply_to_remote(jencode(reply))
        

if __name__=="__main__":
    # ==ALWAYS== INITIATE libghfu before you use it
    libghfu.init(os.path.join(path,"lib"), os.path.join(path,"files","saves")) 

    if libghfu.account_id(libghfu.get_account_by_id(1))==0:
        # create contemporary member...to act as first member in case theere are no members yet in structure
        libghfu.register_new_member(0, "PSEUDO-ROOT",
            c_float.in_dll(libghfu, "ACCOUNT_CREATION_FEE").value + 
            c_float.in_dll(libghfu, "ANNUAL_SUBSCRIPTION_FEE").value,
            file_path("pseudo-root"))
        print "created pseudo-root account to be used (no saved data found!)"
    else:
        print "found saved data, using that..."

    # please install pyopenssl in order to achieve SSL encryption (we used the <ssl_context> parameter in
    # our server run parameters)
    # the .pem certificate files were generated by 
    #   $ openssl req -x509 -newkey rsa:4096 -nodes -out cert.pem -keyout key.pem -days 36500 # 100 yrs=36500 days
    # carefull with the <Common Name (e.g. server FQDN or YOUR name)>. use the exact IP address where the
    # server will be running!
    # 
    # also, when using requests to test the site, use verify='cert.pem' to certify the content from the server!
    #app.run("0.0.0.0", 54321, threaded=1, debug=1, ssl_context=('cert.pem', 'key.pem'))

    finance_server_port = 54322

    # start thread that monitors the exchange rate...
    threading.Thread(target=fetch_current_exchange_rate, args=()).start()

    app.run("0.0.0.0", 54321, threaded=1, debug=1)

