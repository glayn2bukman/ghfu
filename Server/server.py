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

from flask import Flask, request, Response
import os, sys, json, threading, time, random
import requests, datetime
import send_mail

path = os.path.realpath(__file__)
path = os.path.split(os.path.split(path)[0])[0]

data_path = "/var/lib/ghfu"

try:
    with open(os.path.join(data_path,".finance_code"),"r") as finance_code_file:
        finance_server_code = finance_code_file.read().strip()
except: sys.exit("cant find finance server code file ({})".format(os.path.join(data_path,".finance_code")))

try:
    with open(os.path.join(data_path,".monthly_operations_code"),"r") as monthly_operations_file:
        monthly_operations_code = monthly_operations_file.read().strip()
except: sys.exit("cant find monthly operations code file ({})".format(os.path.join(data_path,".finance_code")))


from ctypes import *

libghfu = CDLL("libjermGHFU.so")

# define libghfu function argtypes (so that we can call them normally and let ctypes do any type conversions)
libghfu.invest.argtypes = [c_long, c_float, c_int, c_int, c_char_p]
libghfu.dump_structure_details.argtype = [c_long, c_char_p]
libghfu.get_account_by_id.argtypes = [c_long]
#libghfu.perform_monthly_operations.argtypes = [(c_float*2)*4, c_char_p]
libghfu.purchase_property.argtypes = [c_long, c_float, c_int, c_char_p]
libghfu.redeem_account_points.argtypes = [c_long, c_float, c_int, c_char_p]
libghfu.register_new_member.argtypes = [c_long, c_char_p, c_float, c_int, c_char_p]
libghfu.set_constant.argtypes = [c_char_p, c_float]
libghfu.save_structure.argtypes = [c_char_p, c_char_p]

libghfu.register_new_service.argtypes = [c_long, c_long, c_char_p, c_float, c_float, c_int, c_char_p]
libghfu.pay_for_consumer_service.argtypes = [c_long, c_long, c_float, c_int, c_char_p]
libghfu.update_service_status.argtypes = [c_long, c_long, c_int, c_char_p]

libghfu.search_investments.argtypes = [c_long, c_long, c_int, c_int, c_char_p, c_char_p]
libghfu.search_services.argtypes = [c_char_p, c_char_p, c_long, c_int, c_int, c_int, c_char_p]

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

TRANSACTION_CHECK_DELAY = 30 # delay for checking if pending transaction has been effected
JPESA_DEPOSIT_CHARGES = .03  # also applies when tranfering funds from jpesa to jpesa
JPESA_WITHDRAW_CHARGES = 500.0

CODES = {} # every time a random code is generated, its stored here to hold transaction data
           # so when the client wants to know about the state of a transaction, i dont querry the 
           # jpesa api but rather, i consult with this dictionary and return whatever the latest status
           # of the transaction is kept here. the moment a code is nolonger needed, its deleted otherwise
           # the server would end up consuming GBs of memory holding un-needed data!

FILE_NOT_FOUND = "failed to access file. is account signed in multiple times"


MINIMUM_WITHDRAW = float(JPESA_WITHDRAW_CHARGES)/(
    c_float.in_dll(libghfu, "WITHDRAW_CHARGE").value*.01*c_int.in_dll(libghfu, "EXCHANGE_RATE").value)
MINIMUM_WITHDRAW = int(MINIMUM_WITHDRAW)+1

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
        return FILE_NOT_FOUND

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
    global MINIMUM_WITHDRAW
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
                    if not libghfu.set_constant("exchange-rate", value):
                        server_log("failed to set exchange-rate to {}".format(value))

                    MINIMUM_WITHDRAW = float(JPESA_WITHDRAW_CHARGES)/(
                        c_float.in_dll(libghfu, "WITHDRAW_CHARGE").value*.01*c_int.in_dll(libghfu, "EXCHANGE_RATE").value)
                    MINIMUM_WITHDRAW = int(MINIMUM_WITHDRAW)+1

                    threading.Thread(target=libghfu.gsave, args=()).start()
                    
            except:
                server_log("is the exchange-rates url right? cnt find the index of \" UGX\" in it")
        except: 
            server_log("the exchange-rates url <{}> was moved".format(exchange_rate_url))
        
        time.sleep(12*60*60)

# define function to keep checking on transaction deposit statuses and effect changes to ghfu if the 
# transaction(deposit/withdraw) is verified. if so, the necesary update to GHFU is effected
def effect_transaction(code, reference, func=None, args=None, logfile=None, update_structure=True):
    "amount is in dollars ie the amount to feed to jermlibghfu.so"
    
    while 1:
        try:
            reply = requests.post("http://0.0.0.0:{}/transaction_status".format(finance_server_port), 
                json={"code":finance_server_code, "ref":reference}).text
            reply = jdecode(reply)
            if not reply["status"]:
                if reply["log"]=="No Record Matching Transaction ID" and func==libghfu.redeem_account_points:
                    # this results from a bug in the jpesa api that always returns 'libghfu.redeem_account_points'
                    # even when the funds have been transfered to the other jpesa account!
                    reply["status"] = True
                    reply["log"] = ""
                    reply["details"] = {"status":"complete","tid":reference}
                else:
                    CODES[code]["status"] = False
                    CODES[code]["actionlog"] = "Operation bounced by client."
                    CODES[code]["delete"] = True
                    break
        except: # finance server is down for some reason...
            server_log("the finance server is down. look into this ASAP!")
            time.sleep(TRANSACTION_CHECK_DELAY)
            continue
        
        if reply["status"] and reply["details"]["status"]=="complete":
            if not logfile:
                logfile = file_path("{}.transaction".format(code))

            func_reply = func(*args)

            if func==libghfu.register_new_member:
                CODES[code]["id"] = func_reply
            
            if info(logfile): 
                CODES[code]["status"] = False
                CODES[code]["actionlog"] = info(logfile)
            else:
                CODES[code]["status"] = True
                CODES[code]["details"] = reply
                CODES[code]["actionlog"] = ""
                if update_structure:
                    threading.Thread(target=libghfu.gsave, args=()).start()

            CODES[code]["delete"] = True
                                    
            rm(logfile)
            
            break
        
        time.sleep(TRANSACTION_CHECK_DELAY)

# function to initialte deposit trancaction to jpesa
def depost_funds_to_jpesa(internal_code, number, amount, func, args, logfile=None, update_structure=True):
    """
    this function should be branched off from the callee ie threaded

    typically, we call the function in the following steps;

    ``
    new_internal_code = get_random_code()
    CODES[new_internal_code] = {"status":False, "actionlog":"pending", "delete":False}
    
    threading.Thread(target=depost_funds_to_jpesa, 
        args=(new_internal_code,"0701173049", 500, func,args),
        kwargs={"logfile":logfile, "update_structure":update_structure}).start()
    
    reply["status"] = True
    reply["code"] = new_internal_code
    ``

    """
    try:
        reply = requests.post("http://0.0.0.0:{}/deposit".format(finance_server_port), 
                json={"code":finance_server_code, "number":number, "amount":amount}).text
        reply = jdecode(reply)
        if not reply["status"]:
            CODES[internal_code]["status"] = False
            CODES[internal_code]["actionlog"] = reply["log"]
            CODES[internal_code]["delete"] = True
        else:
            effect_transaction(internal_code,reply["ref"],func, args, logfile, update_structure)
    except:
        CODES[internal_code]["status"] = False
        CODES[internal_code]["actionlog"] = "Failed to reach finance server"

# function to initialte funds transfer from GHFU jpesa account to another jpesa account
def transfer_funds_to_other_jpesa_account(
    internal_code, email, amount, func, args, logfile=None, update_structure=True):
    """
    read the doctsring for function <depost_funds_to_jpesa> above to understand this logic
    """
    try:
        reply = requests.post("http://0.0.0.0:{}/transfer_funds".format(finance_server_port), 
                json={"code":finance_server_code, "email":email, "amount":amount}).text
        reply = jdecode(reply)
        if not reply["status"]:
            CODES[internal_code]["status"] = False
            CODES[internal_code]["actionlog"] = reply["log"]
            CODES[internal_code]["delete"] = True
        else:
            effect_transaction(internal_code,reply["ref"],func, args, logfile, update_structure)
    except:
        CODES[internal_code]["status"] = False
        CODES[internal_code]["actionlog"] = "Failed to reach finance server"

# function to initialte funds transfer from GHFU jpesa account to another jpesa account
def transfer_funds_to_mobile_money(
    internal_code, number, amount, func, args, logfile=None, update_structure=True):
    """
    read the doctsring for function <depost_funds_to_jpesa> above to understand this logic
    """
    try:
        reply = requests.post("http://0.0.0.0:{}/transfer_funds_to_mobile_money".format(finance_server_port), 
                json={"code":finance_server_code, "number":number, "amount":amount}).text
        reply = jdecode(reply)
        if not reply["status"]:
            CODES[internal_code]["status"] = False
            CODES[internal_code]["actionlog"] = reply["log"]
            CODES[internal_code]["delete"] = True
        else:
            effect_transaction(internal_code,reply["ref"],func, args, logfile, update_structure)
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
        number = str(json_req.get("number", ""))

    else:
        uplink_id = request.form.get("uplink",-1)
        names = request.form.get("names",-1)
        deposit = request.form.get("deposit",-1) 
        number = str(request.form.get("number", ""))

        try:
            uplink_id = int(uplink_id)
            deposit = float(deposit)
        except:
            reply["log"] = "silly form data provided for uplink(id) or deposit"
            return reply_to_remote(jencode(reply))

    if (uplink_id<0 or type(uplink_id)!=type(0)):
        reply["log"] = "silly data provided; parameter <uplink_id>"
        return reply_to_remote(jencode(reply))
    if not names:
        reply["log"] = "silly data provided; parameter <names>"
        return reply_to_remote(jencode(reply))
    if (deposit<0 or isinstance(deposit,unicode) or (not (type(deposit)!=type(0) or type(deposit)!=type(0.0)))):
        reply["log"] = "silly data provided; parameter <deposit>"
        return reply_to_remote(jencode(reply))
    if not number:
        reply["log"] = "silly data provided; parameter <number>"
        return reply_to_remote(jencode(reply))
    
    # allow creating accounts with ROOT uplinks? i doubt 
    if uplink_id==0:
        reply["log"] = "you dont have the permission to add accounts to ROOT!"
        return reply_to_remote(jencode(reply))
    else:
        logfile = file_path("{}{}{}.reg".format(uplink_id,names,deposit))

        dummy_id = libghfu.register_new_member(uplink_id, names, deposit, 1, logfile)

        if (not dummy_id) and ((not info(logfile)) or (info(logfile)==FILE_NOT_FOUND)):
            rm(logfile)
            if deposit:
                new_internal_code = get_random_code()
                CODES[new_internal_code] = {"status":False, "actionlog":"pending", "delete":False}
                
                er = c_int.in_dll(libghfu, "EXCHANGE_RATE").value
                ri = c_int.in_dll(libghfu, "RATE_INFLATE").value

                threading.Thread(target=depost_funds_to_jpesa, 
                    args=(new_internal_code,number, deposit*(er+ri), 
                        libghfu.register_new_member,(uplink_id, names, deposit, 0, logfile)),
                    kwargs={"logfile":logfile, "update_structure":True}).start()
                
                reply["status"] = True
                reply["code"] = new_internal_code
            else:
                new_id = libghfu.register_new_member(uplink_id, names, deposit, 0, logfile)
                if info(logfile): 
                    reply["log"] = info(logfile)
                else:
                    reply["id"] = new_id
                rm(logfile)
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
            "commissions": [[str date, float amount, str reason],...],
            "leg_volumes": [float leg-1-volume, float leg-2-volume, float leg-3-volume],
            "investments": [
                [
                    str date, 
                    float points, 
                    str package, 
                    int package id, 
                    int months_returned, 
                    float returns,
                    float total_returns_at_investment_creation],
                ...],
            "direct_children":[str child1_names, str child2_names,.....] /* names NOT ids, again, its for 
                                                                            for security reasons */
            "withdraws":[[str date, float amount],...],
            "creation-date": str,
            "first-month-pv": float
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
    
    if(account_id<0 or type(account_id)!=type(0) or isinstance(account_id,unicode) ):
        reply["log"] = "silly data provided; parameter <id>"
        return reply_to_remote(jencode(reply))
    
    jsonfile = os.path.join(path, "files","json","{}.json".format(account_id))
    
    if libghfu.dump_structure_details(account_id, jsonfile):
        try:
            with open(jsonfile, "r") as f: reply = f.read()
        except:
            reply["log"] = "failed to access json file. is account signed in multiple times"
    else:
        reply["log"] = "no account matching requested target!"
        reply = jencode(reply)

    rm(jsonfile)
    
    return reply_to_remote(reply)

@app.route("/buy_package", methods=["POST"])
def buy_package():
    " if returned json <status> key is true, all went well, otherwise, check <log>"

    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()
    
    if json_req:
        IB_id = json_req.get("id",-1)
        amount = json_req.get("amount",0)
        number = str(json_req.get("number", ""))

    else:
        IB_id = request.form.get("id",-1)
        amount = request.form.get("amount",-1)
        number = str(request.form.get("number", ""))

        try:
            IB_id = int(IB_id)
            amount = float(amount)
        except:
            reply["log"] = "silly form data provided"
            return reply_to_remote(jencode(reply))

    if(IB_id<0 or type(IB_id)!=type(0) or isinstance(IB_id,unicode) ):
        reply["log"] = "silly data provided; parameter <id>"
        return reply_to_remote(jencode(reply))
    if(amount==0 or (not (type(amount)==type(0.0) or type(amount)==type(0))) or isinstance(amount,unicode) ):
        reply["log"] = "silly data provided; parameter <amount>"
        return reply_to_remote(jencode(reply))
    if not number:
        reply["log"] = "silly data provided; parameter <number>"
        return reply_to_remote(jencode(reply))

    logfile = file_path("{}{}.buy_pkg".format(IB_id,amount))

    if libghfu.purchase_property(IB_id, c_float(amount), 1, logfile):
        rm(logfile)
        new_internal_code = get_random_code()
        CODES[new_internal_code] = {"status":False, "actionlog":"pending", "delete":False}
        
        # for services and property, the amount is in UGX and not UDS like investments and registration
        fper = c_int.in_dll(libghfu, "FIXED_PROPERTY_EXCHANGE_RATE").value

        threading.Thread(target=depost_funds_to_jpesa, 
            args=(new_internal_code,number, amount, 
                libghfu.purchase_property,(IB_id, c_float(amount/float(fper)), 0, logfile)),
            kwargs={"logfile":logfile, "update_structure":True}).start()
        
        reply["status"] = True
        reply["code"] = new_internal_code
    else:
        reply["log"] = info(logfile)
        rm(logfile)

    return reply_to_remote(jencode(reply))

@app.route("/data_constants", methods=["POST"])
def get_data_constants():
    """ return data constants ie;
        POINT_FACTOR,PAYMENT_DAY,ACCOUNT_CREATION_FEE,ANNUAL_SUBSCRIPTION_FEE,
        OPERATIONS_FEE,MINIMUM_INVESTMENT,MAXIMUM_INVESTMENT,LAST_INVESTMENT_DAY,
        EXCHANGE_RATE, WITHDRAW_CHARGE,RATE_INFLATE, MINIMUM_WITHDRAW
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
    reply["last-investment-day"] = c_int.in_dll(libghfu, "LAST_INVESTMENT_DAY").value
    reply["exchange-rate"] = c_int.in_dll(libghfu, "EXCHANGE_RATE").value
    reply["withdraw-charge"] = c_float.in_dll(libghfu, "WITHDRAW_CHARGE").value
    reply["rate-inflate"] = c_int.in_dll(libghfu, "RATE_INFLATE").value
    reply["minimum-withdraw"] = MINIMUM_WITHDRAW
    reply["available-investments"] = c_long.in_dll(libghfu, "AVAILABLE_INVESTMENTS").value

    return reply_to_remote(jencode(reply))

@app.route("/set_data_constants", methods=["POST"])
def set_data_constants():
    """ set data constants ie;
        POINT_FACTOR,PAYMENT_DAY,ACCOUNT_CREATION_FEE,ANNUAL_SUBSCRIPTION_FEE,
        OPERATIONS_FEE,MINIMUM_INVESTMENT,MAXIMUM_INVESTMENT, LAST_INVESTMENT_DAY,
        EXCHANGE_RATE, WITHDRAW_CHARGE,RATE_INFLATE
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
                if key=="available-investments" and request.access_route[-1]!="127.0.0.1":
                    # only local-host can edit this information...
                    reply[key]=False
                    continue
                reply[key] = libghfu.set_constant(key, float(json_req[key]))
                reply[key] = True if reply[key] else False
            else: reply[key]=False
    else:
        for key in request.form:
            try:
                value = float(request.form[key])
                if key=="available-investments" and request.access_route[-1]!="127.0.0.1":
                    # only local-host can edit this information...
                    reply[key]=False
                    continue
                reply[key] = libghfu.set_constant(key, value)
                reply[key] = True if reply[key] else False
            except: reply[key]=False

    for key in reply:
        if reply[key]:

            threading.Thread(target=libghfu.gsave, args=()).start()

            break

    global MINIMUM_WITHDRAW
    MINIMUM_WITHDRAW = float(JPESA_WITHDRAW_CHARGES)/(
        c_float.in_dll(libghfu, "WITHDRAW_CHARGE").value*.01*c_int.in_dll(libghfu, "EXCHANGE_RATE").value)
    MINIMUM_WITHDRAW = int(MINIMUM_WITHDRAW)+1

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
        number = str(json_req.get("number", ""))

    else:
        account_id = request.form.get("id",-1)
        amount = request.form.get("amount", -1)
        number = str(request.form.get("number", ""))

        try:
            account_id = int(account_id)
            amount = float(amount)
        except:
            reply["log"] = "silly form data provided"
            return reply_to_remote(jencode(reply))
            
    if(account_id<0 or type(account_id)!=type(0) or isinstance(account_id,unicode) ):
        reply["log"] = "silly data provided; parameter <account_id>"
        return reply_to_remote(jencode(reply))
    if(amount<0 or (not(type(amount)!=type(0) or type(amount)!=type(0.0))) or isinstance(amount,unicode) ):
        reply["log"] = "silly data provided; parameter <amount>"
        return reply_to_remote(jencode(reply))
    if not number:
        reply["log"] = "silly data provided; parameter <number>"
        return reply_to_remote(jencode(reply))


    logfile = file_path("{}{}.inv".format(account_id,amount))

    if libghfu.invest(account_id, amount, 1, 1,logfile):
        rm(logfile)
        new_internal_code = get_random_code()
        CODES[new_internal_code] = {"status":False, "actionlog":"pending", "delete":False}
        
        er = c_int.in_dll(libghfu, "EXCHANGE_RATE").value
        ri = c_int.in_dll(libghfu, "RATE_INFLATE").value

        threading.Thread(target=depost_funds_to_jpesa, 
            args=(new_internal_code,number, amount*(er+ri), 
                libghfu.invest,(account_id, amount, 1,0, logfile)),
            kwargs={"logfile":logfile, "update_structure":True}).start()
        
        reply["status"] = True
        reply["code"] = new_internal_code
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

    if (not ID) or ID!=monthly_operations_code:
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

        threading.Thread(target=libghfu.gsave, args=()).start()

        return reply_to_remote(jencode({"status":True, "log":""}))

    log = info(logfile)
    rm(logfile)

    return reply_to_remote(jencode({"status":False, "log":log}))


@app.route("/transaction_status", methods=["POST"])
def check_transaction_status():
    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()
    
    if not json_req:
        reply["log"] = "server expects json here"
        return reply_to_remote(jencode(reply))

    code = json_req.get("code","")
    
    code = str(code)
    
    if not code:
        reply["log"] = "silly reqest given, expected a code"
        return reply_to_remote(jencode(reply))

    transaction_data = CODES.get(code,None)
    
    if not transaction_data:
        reply["log"] = "your code is not in the system"
        return reply_to_remote(jencode(reply))

    if "id" in transaction_data: # url /register
        reply["id"] = transaction_data["id"]

    if not transaction_data["status"]:
        reply["log"] = transaction_data["actionlog"]
    else:
        reply["status"]=True
        reply["new_code"] = transaction_data["details"]["details"]["tid"][:8]

    if transaction_data["delete"]:
        del(CODES[code])

    return reply_to_remote(jencode(reply))

@app.route("/transfer_funds", methods=["POST"])
def transfer_funds_to_client_jpesa_account():
    " if returned json <status> key is true, all went well, otherwise, check <log>"

    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()
    
    if json_req:
        account_id = json_req.get("id", -1)
        amount = json_req.get("amount", -1)
        email = str(json_req.get("email", ""))

    else:
        account_id = request.form.get("id",-1)
        amount = request.form.get("amount", -1)
        email = str(request.form.get("email", ""))

        try:
            account_id = int(account_id)
            amount = float(amount)
        except:
            reply["log"] = "silly form data provided"
            return reply_to_remote(jencode(reply))
            
    if(account_id<0 or type(account_id)!=type(0) or isinstance(account_id,unicode) ):
        reply["log"] = "silly data provided; parameter <account_id>"
        return reply_to_remote(jencode(reply))
    if(amount<0 or (not(type(amount)!=type(0) or type(amount)!=type(0.0))) or isinstance(amount,unicode) ):
        reply["log"] = "silly data provided; parameter <amount>"
        return reply_to_remote(jencode(reply))
    if not email:
        reply["log"] = "silly data provided; parameter <email>"
        return reply_to_remote(jencode(reply))
    if amount<MINIMUM_WITHDRAW:
        reply["log"] = "withdraw less than minimum of ${}".format(MINIMUM_WITHDRAW)
        return reply_to_remote(jencode(reply))


    logfile = file_path("{}{}.fundstransfer".format(account_id,amount))

    if libghfu.redeem_account_points(account_id, amount, 1, logfile):
        rm(logfile)
        new_internal_code = get_random_code()
        CODES[new_internal_code] = {"status":False, "actionlog":"pending", "delete":False}

        er = c_int.in_dll(libghfu, "EXCHANGE_RATE").value
        wc = c_float.in_dll(libghfu, "WITHDRAW_CHARGE").value
        ri = c_int.in_dll(libghfu, "RATE_INFLATE").value
        
        threading.Thread(target=transfer_funds_to_other_jpesa_account, 
            args=(new_internal_code,email, amount*(er-ri)*(100-wc+(JPESA_DEPOSIT_CHARGES*100))/100., 
                libghfu.redeem_account_points, (account_id, amount, 0, logfile)),
            kwargs={"logfile":logfile, "update_structure":True}).start()
        
        reply["status"] = True
        reply["code"] = new_internal_code
    else:
        reply["log"] = info(logfile)
        rm(logfile)

    return reply_to_remote(jencode(reply))

@app.route("/transfer_funds_to_mobile_money", methods=["POST"])
def transfer_funds_from_jpesa_to_mobile_money():
    " if returned json <status> key is true, all went well, otherwise, check <log>"

    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()
    
    if json_req:
        account_id = json_req.get("id", -1)
        amount = json_req.get("amount", -1)
        number = str(json_req.get("number", ""))

    else:
        account_id = request.form.get("id",-1)
        amount = request.form.get("amount", -1)
        number = str(request.form.get("number", ""))

        try:
            account_id = int(account_id)
            amount = float(amount)
        except:
            reply["log"] = "silly form data provided"
            return reply_to_remote(jencode(reply))
            
    if(account_id<0 or type(account_id)!=type(0) or isinstance(account_id,unicode) ):
        reply["log"] = "silly data provided; parameter <account_id>"
        return reply_to_remote(jencode(reply))
    if(amount<0 or (not(type(amount)!=type(0) or type(amount)!=type(0.0))) or isinstance(amount,unicode) ):
        reply["log"] = "silly data provided; parameter <amount>"
        return reply_to_remote(jencode(reply))
    if not number:
        reply["log"] = "silly data provided; parameter <number>"
        return reply_to_remote(jencode(reply))
    if amount<MINIMUM_WITHDRAW:
        reply["log"] = "withdraw less than minimum of ${}".format(MINIMUM_WITHDRAW)
        return reply_to_remote(jencode(reply))

    logfile = file_path("{}{}.fundstransfertoMM".format(account_id,amount))

    if libghfu.redeem_account_points(account_id, amount, 1, logfile):
        rm(logfile)
        new_internal_code = get_random_code()
        CODES[new_internal_code] = {"status":False, "actionlog":"pending", "delete":False}

        er = c_int.in_dll(libghfu, "EXCHANGE_RATE").value
        wc = c_float.in_dll(libghfu, "WITHDRAW_CHARGE").value
        ri = c_int.in_dll(libghfu, "RATE_INFLATE").value
        
        threading.Thread(target=transfer_funds_to_mobile_money, 
            args=(new_internal_code,number, JPESA_WITHDRAW_CHARGES+(amount*(er-ri)*(100-wc)/100.), 
                libghfu.redeem_account_points, (account_id, amount, 0, logfile)),
            kwargs={"logfile":logfile, "update_structure":True}).start()
        
        reply["status"] = True
        reply["code"] = new_internal_code
    else:
        reply["log"] = info(logfile)
        rm(logfile)

    return reply_to_remote(jencode(reply))

@app.route("/search_investments", methods=["POST"])
def search_investments():
    """
    if sucessfull, reply;
    {
        "status":True,
        "log":"",
        "results":[
            [str invesmtne-date, str investment type, float amount, int months-returned],...
        ]
    }
    """
    
    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()
    if json_req==None: json_req={}
        
    date_format = str(json_req.get("date_format","dd/mm/yyyy"))
    search_from = str(json_req.get("search_from",""))
    search_to = str(json_req.get("search_to",""))
    months_returned_from = json_req.get("months_returned_from",0)
    months_returned_to = json_req.get("months_returned_to",0)
    investment_type = str(json_req.get("investment_type","all"))
    
    if search_from and (search_from.count("/")!=2):
        reply["log"] = "silly date given for <search_from>"
        return reply_to_remote(jencode(reply))
    if search_to and (search_to.count("/")!=2):
        reply["log"] = "silly date given for <search_to>"
        return reply_to_remote(jencode(reply))
    
    if search_from:
        try:
            search_from = [int(fig) for fig in search_from.split("/")]
        except:
            reply["log"] = "silly date given for <search_from>"
            return reply_to_remote(jencode(reply))
    else: search_from = 0

    if search_to:
        try:
            search_to = [int(fig) for fig in search_to.split("/")]
        except:
            reply["log"] = "silly date given for <search_to>"
            return reply_to_remote(jencode(reply))
    else: search_to = 0
    
    if search_from:
        if date_format=="dd/mm/yyyy":
            search_from.reverse()
            search_from = int(datetime.datetime(*search_from).strftime("%s"))
        else: # mm/dd/yyyy -> american style
            search_from = int(datetime.datetime(search_from[2],search_from[0],search_from[1]).strftime("%s"))

    if search_to:
        if date_format=="dd/mm/yyyy":
            search_to.reverse()
            search_to = int(datetime.datetime(*search_to).strftime("%s"))
        else: # mm/dd/yyyy -> american style
            search_to = int(datetime.datetime(search_to[2],search_to[0],search_to[1]).strftime("%s"))

    try:
        months_returned_from = int(months_returned_from)
        months_returned_to = int(months_returned_to)
        
        if months_returned_from>12 or months_returned_to>12 or \
            months_returned_from<0 or months_returned_to<0:
            reply["log"] = "silly date given. <months_returned_from> or <months_returned_to> range from 1 to 12"
            return reply_to_remote(jencode(reply))
        
    except:
        reply["log"] = "silly date given for <months_returned_from> or <months_returned_to>"
        return reply_to_remote(jencode(reply))

    logfile = file_path("{}{}{}.search_invs.json".format(time.time(),months_returned_from,months_returned_to))

    """
    search_investments(const time_t search_from, const time_t search_to, 
    const unsigned int months_returned_from, const unsigned int months_returned_to,
    const String type, const String fout_name)
    """
    
    if libghfu.search_investments(search_from, search_to, months_returned_from, months_returned_to,
        investment_type, logfile):
        results = jdecode(info(logfile))
        reply["status"] = True
        reply["results"] = results["results"]
    else:
        reply["log"] = info(logfile)
    
    rm(logfile)

    
    return reply_to_remote(jencode(reply))

@app.route("/search_services", methods=["POST"])
def search_services():
    """
        if sucessfull, reply;
        {
            "status":True,
            "log":"",
            "results":{
                "0":[str consumer-name, str service-name, int service-id, 
                    float service-price, str last-payment-date, str next-payment-date],
            }
        }
    
    """
    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()
    if json_req==None: json_req={}

    consumer_name = str(json_req.get("consumer_name","all"))
    service_name = str(json_req.get("service_name","all"))
    date_format = str(json_req.get("date_format","dd/mm/yyyy"))
    service_acquisition_date = str(json_req.get("acquisition_date",""))
    
    # the bools below are either True,False,"". "" represents <neutral> ie any of True/False.
    # this modification is made in libghfu to handle scenarios where we dont care if a value is true or 
    # false during the search...
    service_paid = json_req.get("service_paid","")
    service_active = json_req.get("service_is_active","")
    security_month_paid = json_req.get("security_month_paid","")

    if service_acquisition_date and (service_acquisition_date.count("/")!=2):
        reply["log"] = "silly date given for <service_acquisition_date>"
        return reply_to_remote(jencode(reply))

    if service_acquisition_date:
        try:
            service_acquisition_date = [int(fig) for fig in service_acquisition_date.split("/")]
        except:
            reply["log"] = "silly date given for <service_acquisition_date>"
            return reply_to_remote(jencode(reply))
        if date_format=="dd/mm/yyyy":
            service_acquisition_date.reverse()
            service_acquisition_date = int(datetime.datetime(*service_acquisition_date).strftime("%s"))
        else: # mm/dd/yyyy -> american style
            service_acquisition_date = int(datetime.datetime(
                service_acquisition_date[2],service_acquisition_date[0],service_acquisition_date[1]).strftime("%s"))

    else: service_acquisition_date = 0
    
    service_paid = 1 if service_paid==True else (0 if service_paid==False else 2)
    service_active = 1 if service_active==True else (0 if service_active==False else 2)
    security_month_paid = 1 if security_month_paid==True else (0 if security_month_paid==False else 2)

    """
    search_services(const String consumer_name, const String service_name, 
        const time_t service_acquisition_date, const bool service_paid, 
        const bool service_active, const bool security_month_paid, const String fout_name)
    """

    logfile = file_path("{}{}.search_services.json".format(time.time(),consumer_name))

    if libghfu.search_services(consumer_name,service_name,service_acquisition_date,
        service_paid,service_active,security_month_paid,logfile):
        results = jdecode(info(logfile))
        reply["results"] = results
        reply["status"] = True
    else:
        reply["log"] = info(logfile)
    
    rm(logfile)

    return reply_to_remote(jencode(reply))


@app.route("/update_service_status", methods=["POST"])
def update_service_status():
    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}
    
    json_req = request.get_json()
    
    if not json_req:
        reply["log"] = "expected json data..."
        return reply_to_remote(jencode(reply))
    
    account_id = json_req.get("id",-1)
    service_id = json_req.get("service_id",-1)
    new_status = json_req.get("new_status",True)
    
    try:
        account_id=int(account_id)
        service_id= int(service_id)
        
        if account_id<0 or service_id<0:
            reply["log"] = "silly data provided... <account_id> or <service_id>"
            return reply_to_remote(jencode(reply))        
    except:
        reply["log"] = "silly data provided... <account_id> or <service_id>"
        return reply_to_remote(jencode(reply))
    
    new_status = 1 if new_status else 0
    
    logfile = file_path("{}{}{}.service_status".format(account_id, service_id, new_status))

    """
    update_service_status(const ID account_id, const ID service_id, const bool new_status, 
        const String fout_name)
    """
    
    if libghfu.update_service_status(account_id, service_id,new_status, logfile):
        reply["status"] = True
        threading.Thread(target=libghfu.gsave, args=()).start()
    else:
        reply["log"] = info(logfile)
    
    rm(logfile)
    
    return reply_to_remote(jencode(reply))

@app.route("/create_new_service", methods=["POST"])
def create_new_service():
    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()
    
    if not json:
        reply["log"] = "expected json data in request..."
        return reply_to_remote(jencode(reply))
    
    number = str(json_req.get("number",""))
    account_id = json_req.get("id",-1)
    service_id = json_req.get("service_id",-1)
    service_name = str(json_req.get("service_name",""))
    service_price = json_req.get("service_price",-1) # UGX NOT USD
    deposit = json_req.get("deposit",0) # UGX NOT USD
    
    try:
        account_id,service_id,deposit,service_price = int(account_id),int(service_id),int(deposit),int(service_price)
        if account_id<0 or service_id<0 or deposit<0 or service_price<0:
            reply["log"] = "silly data provided for <account_id>,<service_id>,<deposit>,<service_price>"
            return reply_to_remote(jencode(reply))
    except:
        reply["log"] = "silly data provided for <account_id>,<service_id>,<deposit>,<service_price>"
        return reply_to_remote(jencode(reply))
        
    if not number:
        reply["log"] = "silly data provided; parameter <number>"
        return reply_to_remote(jencode(reply))

    logfile = file_path("{}{}{}.new_service".format(account_id,number,service_name,service_id))

    """
    register_new_service(ID account_id, const ID service_id, const String service_name, 
        const Amount unit_price, const Amount amount, const bool test_feasibility, String fout_name)
    """

    # for services and property, the amount is in UGX and not UDS like investments and registration
    fper = c_int.in_dll(libghfu, "FIXED_PROPERTY_EXCHANGE_RATE").value

    if libghfu.register_new_service(account_id, service_id, service_name, c_float(service_price/float(fper)),c_float(deposit/float(fper)),1,logfile):
        rm(logfile)
        new_internal_code = get_random_code()
        CODES[new_internal_code] = {"status":False, "actionlog":"pending", "delete":False}
        
        threading.Thread(target=depost_funds_to_jpesa, 
            args=(new_internal_code,number, deposit, 
                libghfu.register_new_service,(account_id, service_id, service_name, c_float(service_price/float(fper)), c_float(deposit/float(fper)),0,logfile)),
            kwargs={"logfile":logfile, "update_structure":True}).start()
        
        reply["status"] = True
        reply["code"] = new_internal_code
    else:
        reply["log"] = info(logfile)
        rm(logfile)

    
    return reply_to_remote(jencode(reply))

@app.route("/pay_for_service", methods=["POST"])
def pay_for_services():
    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()

    number = str(json_req.get("number",""))
    account_id = json_req.get("id",-1)
    service_id = json_req.get("service_id",-1)
    deposit = json_req.get("deposit",0) # UGX NOT USD

    try:
        account_id,service_id,deposit = int(account_id),int(service_id),int(deposit)
        if account_id<0 or service_id<0 or deposit<0:
            reply["log"] = "silly data provided for <account_id>,<service_id>,<deposit>"
            return reply_to_remote(jencode(reply))
    except:
        reply["log"] = "silly data provided for <account_id>,<service_id>,<deposit>"
        return reply_to_remote(jencode(reply))
        
    if not number:
        reply["log"] = "silly data provided; parameter <number>"
        return reply_to_remote(jencode(reply))

    logfile = file_path("{}{}{}.pay_service".format(account_id,number,service_id))


    # for services and property, the amount is in UGX and not UDS like investments and registration
    fper = c_int.in_dll(libghfu, "FIXED_PROPERTY_EXCHANGE_RATE").value

    """
    pay_for_consumer_service(ID account_id, const ID service_id, const Amount amount,
        const bool test_feasibility, const String fout_name);
    """

    if libghfu.pay_for_consumer_service(account_id, service_id, c_float(deposit/float(fper)), 1, logfile):
        rm(logfile)
        new_internal_code = get_random_code()
        CODES[new_internal_code] = {"status":False, "actionlog":"pending", "delete":False}
        
        threading.Thread(target=depost_funds_to_jpesa, 
            args=(new_internal_code,number, deposit, 
                libghfu.pay_for_consumer_service,(account_id, service_id, c_float(deposit/float(fper)),0,logfile)),
            kwargs={"logfile":logfile, "update_structure":True}).start()
        
        reply["status"] = True
        reply["code"] = new_internal_code
    else:
        reply["log"] = info(logfile)
        rm(logfile)


    return reply_to_remote(jencode(reply))

@app.route("/get_registration_code", methods=["POST"])
def get_register_code():
    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()

    if not json_req:
        reply["log"] = "expected json data in request"
        return reply_to_remote(jencode(reply))
    
    email = str(json_req.get("email",""))
    
    if (not email) or (not ("@" in email)):
        reply["log"] = "invalid email provided"

    temp_code = get_random_code()

    reply["status"] = True
    reply["email"] = email
    reply["code"] = temp_code
    
    threading.Thread(target=send_mail.send_mail, args=("GHFU CODE",[],[email],
        """
Greetings,

Thanks for registering with GHFU.

Your Code is: {}

Visit the app, use your email as the username and the code given above as the password to login

Thank you        """.format(temp_code))).start()
    
    return reply_to_remote(jencode(reply))

@app.route("/email", methods=["POST"])
def email():
    if not client_known(request.access_route[-1]): 
        return reply_to_remote("You are not authorised to access this server!"),401
    
    reply = {"status": False, "log":""}

    json_req = request.get_json()

    email = json_req.get("email","")
    subject = json_req.get("subject","")
    message = json_req.get("message","")
    send_code = json_req.get("send_code",False)

    if not email:
        reply["log"] = "please provide the target email address"
        return reply_to_remote(jencode(reply))

    if (not send_code) and ((not subject) or (not message)): # idiot forgot their email...
        reply["log"] = "please provide a subject and message for the email"
        return reply_to_remote(jencode(reply))
    
    reply["email"] = email
        
    if send_code:
        temp_code = get_random_code()
        reply["status"] = True
        reply["code"] = temp_code

        threading.Thread(target=send_mail.send_mail, args=("GHFU RECOVERY PASSWORD",[],[email],
"""
Greetings,

Your new password is: {}

Please use your username : your_email and the new password to login into your account

You can change your password after you login into your account using the change password link.

Thank you for choosing GHFU.
""".format(temp_code))).start()
    
    else:
        reply["status"] = True
        threading.Thread(target=send_mail.send_mail, args=(subject,[],[email],message)).start()


    return reply_to_remote(jencode(reply))


if __name__=="__main__":
    # ==ALWAYS== INITIATE libghfu before you use it
    libghfu.init(os.path.join(path,"lib"), os.path.join(path,"files","saves")) 

    if libghfu.account_id(libghfu.get_account_by_id(1))==0:
        # create contemporary member...to act as first member in case theere are no members yet in structure
        libghfu.register_new_member(0, "PSEUDO-ROOT",
            c_float.in_dll(libghfu, "ACCOUNT_CREATION_FEE").value,
            0,
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

