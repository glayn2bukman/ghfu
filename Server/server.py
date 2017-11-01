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
import os, sys, json, time

from ctypes import *

path = os.path.split(os.path.realpath(__name__))[0]

libghfu = CDLL(os.path.join(path,"lib","libjermGHFU.so"))

# define libghfu function argtypes (so that we can call them normally and let ctypes do any type conversions)
libghfu.invest.argtypes = [c_long, c_float, c_char_p, c_long, c_int, c_char_p]
libghfu.dump_structure_details.argtype = [c_long, c_char_p]
libghfu.get_account_by_id.argtypes = [c_long]
libghfu.perform_monthly_operations.argtypes = [(c_float*2)*4, c_char_p]
libghfu.purchase_property.argtypes = [c_long, c_float, c_int, c_char_p, c_char_p]
libghfu.redeem_account_points.argtypes = [c_long, c_float, c_char_p]
libghfu.register_new_member.argtypes = [c_long, c_char_p, c_float, c_char_p]

# libghfu.perform_monthly_operations is called as follows
#   data = [(375,64),(250,60),(125,49),(0,0)]
#   libghfu.perform_monthly_operations(((c_float*2)*4)(*data), "fout")

#   NB data is a LIST of TUPLES. and the last TUPLE MUST BE (0,0) as this is the terminating 
#      condition in libghfu

# define Account(struct account), AccountPointer (struct account_pointer), Commission(struct commission)
# and Investment (struct investment); libghfu struct derivatives for python (will save account-search time)
"""
class Investment(Structure):pass
Investment._fields_ = [
    ("date",c_long), ("amount",c_float), ("package", c_char_p), ("package_id", c_long),
    ("returns", c_float), ("months_returned", c_int), ("next",POINTER(Investment)),
    ("next",POINTER(Investment))
]

class Commission(Structure):pass
Commission._fields_ = [
    ("reason",c_char_p), ("amount",c_float),("next",POINTER(Commission)),("prev",POINTER(Commission))
]

class AccountPointer(Structure):pass
AccountPointer._fields_ = [
    ("account",c_void_p), ("next",POINTER(AccountPointer)), ("prev",POINTER(AccountPointer))
]

class Account(Structure):pass
Account._fields_ = [
    ("id",c_long), ("names",c_char_p),("pv",c_float),("available_balance",c_float),
    ("total_returns",c_float), ("total_redeems", c_float),("uplink", POINTER(Account)),
    ("children",POINTER(AccountPointer)), ("last_child",POINTER(AccountPointer)),
    ("commissions",POINTER(Commission)), ("last_commission",POINTER(Commission)),
    ("leg_volumes", c_float*3), ("TVC_levels",c_float*2), ("investments",POINTER(Investment)),
    ("last_investment",POINTER(Investment)), ("rank",c_int), ("highest_leg_ranks",c_int*3)
] 
"""

app = Flask(__name__)

known_clients = "127.0.0.1"
jencode = json.JSONEncoder().encode
jdecode = json.JSONDecoder().decode

# remove used uncessesary file
def rm(f):
    os.remove(f)

# modify file path
def file_path(fname):
    return os.path.join(path, "files","out",fname)

# function to fetch logs/errors
def info(fname):
    with open(file_path(fname),"r") as f: 
        return f.read()

# define a function that wil allow responses to be sent to pages not served by this server
def reply_to_remote(reply):
    response = Response(reply)
    response.headers["Access-Control-Allow-Origin"] = "*" # allow all domains...
    return response

# define scanning function (we ony wanna accept requests from known clients)
def client_known(addr):
    if not(os.path.isfile(os.path.join(path,"Server","known_clients"))):
        with open(os.path.join(path,"Server","known_clients"), "w") as f: f.write(known_clients)
    with open(os.path.join(path,"Server","known_clients"), "r") as f:
        known = f.read().strip().split("\n")
        for client in known:
            if addr==client: return True
    return False

@app.route("/test",methods=["GET","POST"])
def test():

    if not client_known(request.remote_addr): 
        return reply_to_remote("You are not authorised to access this server!"),401
    reply = "Server is up!"
    
    return reply_to_remote(reply)
    

@app.route("/register",methods=["POST"])
def register():
    """ if returned json has <id> set to 0, check for the log from key <log>"""

    if not client_known(request.remote_addr): 
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
    if (deposit==-1 or isinstance(deposit,unicode) or (not (type(deposit)!=type(0) or type(deposit)!=type(0.0)))):
        reply["log"] = "silly data provided; parameter <deposit>"
        return reply_to_remote(jencode(reply))
    
    if uplink_id and (not libghfu.account_id(libghfu.get_account_by_id(uplink_id))):
        reply["log"] = "uplink does not exist!"
        return reply_to_remote(jencode(reply))

    # allow creating accounts with ROOT uplinks? i doubt 
    elif uplink_id==0:
        reply["log"] = "you dont have the permission to add accounts to ROOT!"
        return reply_to_remote(jencode(reply))
    else:
        logfile = file_path("{}".format(time.time()))

        account_id = libghfu.register_new_member(uplink_id, names, deposit, logfile)
        if account_id: reply["id"]=account_id
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
            "investments": [[str date, float points, str package, int months_returned],...],
            "direct_children":[str child1_names, str child2_names,.....] /* names NOT ids, again, its for 
                                                                            for security reasons */
        }
    
    """

    if not client_known(request.remote_addr): 
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
    
    jsonfile = os.path.join(path, "files","json","{}".format(time.time()))
        
    if libghfu.dump_structure_details(account_id, jsonfile):
        with open(jsonfile, "r") as f: reply = f.read()
        rm(jsonfile)
    else:
        reply["log"] = "no account matching requested target!"
        reply = jencode(reply)
    
    return reply_to_remote(reply)

@app.route("/buy_package", methods=["POST"])
def buy_package():
    " if returned json <status> key is true, all went well, otherwise, check <log>"

    if not client_known(request.remote_addr): 
        return reply_to_remote("You are not authorised to access this server!"),401

    reply = {"status":False, "log":""}

    json_req = request.get_json()
    
    if json_req:
        IB_id = json_req.get("IB_id",-1)
        amount = json_req.get("amount",-1)
        is_member = json_req.get("buyer_is_member", 0)
        buyer_names = str(json_req.get("buyer_names", "bought package")) # use packag name if registered member is the one 
                                                                    # buying
        is_member = 1 if is_member else 0 # convert from True/False to 1/0 (C daint have bools)

    else:
        IB_id = request.form.get("IB_id",-1)
        amount = request.form.get("amount",-1)
        is_member = request.form.get("buyer_is_member", 0)
        buyer_names = request.form.get("buyer_names", "bought package")

        try:
            IB_id = int(IB_id)
            amount = float(amount)
            is_member = 1 if is_member=="true" else 0
        except:
            reply["log"] = "silly form data provided"
            return reply_to_remote(jencode(reply))

    if(IB_id==-1 or type(IB_id)!=type(0) or isinstance(IB_id,unicode) ):
        reply["log"] = "silly data provided; parameter <IB_id>"
        return reply_to_remote(jencode(reply))
    if(amount==-1 or (not (type(amount)==type(0.0) or type(amount)==type(0))) or isinstance(amount,unicode) ):
        reply["log"] = "silly data provided; parameter <amount>"
        return reply_to_remote(jencode(reply))

    logfile = file_path("{}".format(time.time()))

    libghfu.purchase_property(IB_id, c_float(amount), is_member, buyer_names, logfile)
    if info(logfile):
        with open(logfile, "r") as f: reply["log"] = f.read()
    else: reply["status"]=True


    rm(logfile)

    return reply_to_remote(jencode(reply))

@app.route("/data_constants", methods=["POST"])
def get_data_constants():
    """ return data constants ie;
        float PF = POINT_FACTOR;
        unsigned int PD = PAYMENT_DAY;

        float ACF = ACCOUNT_CREATION_FEE;
        float ASF = ANNUAL_SUBSCRIPTION_FEE;
        float OF = OPERATIONS_FEE;
        float MinI = MINIMUM_INVESTMENT;
        float MaxI = MAXIMUM_INVESTMENT;
    """

    if not client_known(request.remote_addr): 
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

@app.route("/invest", methods=["POST"])
def invest():
    " if returned json <status> key is true, all went well, otherwise, check <log>"

    if not client_known(request.remote_addr): 
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

    logfile = file_path("{}".format(time.time()))

    package = str(package)
    
    if libghfu.invest(account_id, amount, package, package_id, 1, logfile):
        reply["status"]=True
    else:
        reply["log"] = info(logfile)

    rm(logfile)

    return reply_to_remote(jencode(reply))

if __name__=="__main__":
    libghfu.init(file_path("server-init")) # ==ALWAYS== INITIATE libghfu before you use it
    init_error = info("server-init")
    if init_error:
        sys.exit(init_error)

    # create contemporary member...to act as first member in case theere are no members yet in structure
    libghfu.register_new_member(0, "PSEUDO-ROOT",
        c_float.in_dll(libghfu, "ACCOUNT_CREATION_FEE").value + 
        c_float.in_dll(libghfu, "ANNUAL_SUBSCRIPTION_FEE").value+180+500,
        file_path("pseudo-root"))

    app.run("0.0.0.0", 54321, threaded=1, debug=1)
