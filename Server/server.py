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
    print addr
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

    if json_req:
        uplink_id = json_req.get("uplink",-1)
        names = str(json_req.get("names",""));
        deposit = json_req.get("deposit",-1)

        if uplink_id==-1:
            reply["log"] = "silly json data provided; parameter <uplink_id>"
            return reply_to_remote(jencode(reply))
        if not names:
            reply["log"] = "silly json data provided; parameter <names>"
            return reply_to_remote(jencode(reply))
        if deposit==-1:
            reply["log"] = "silly json data provided; parameter <deposit>"
            return reply_to_remote(jencode(reply))

    else:
        uplink_id = request.form["uplink"]
        names = request.form["names"]
        deposit = request.form["deposit"] # registration,anual server charges + operations fee if investing

        try:
            uplink_id = int(uplink_id)
            deposit = float(deposit)
        except:
            reply["log"] = "silly form data provided for uplink(id) or deposit"
            return reply_to_remote(jencode(reply))
        
    logfile = file_path("{}".format(time.time()))

    if uplink_id and (not libghfu.account_id(libghfu.get_account_by_id(uplink_id))):
        reply["log"] = "uplink does not exist!"

    # allow creating accounts with ROOT uplinks? i doubt 
    elif uplink_id==0:
        reply["log"] = "you dont have the permission to add accounts to ROOT!"
    else:
        account_id = libghfu.register_new_member(uplink_id,names,c_float(deposit),logfile)
        if account_id: reply["id"]=account_id
        else: 
            reply["log"] = info(logname)

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
        if(account_id==-1):
            reply["log"] = "silly json data provided; parameter <id>"
            return reply_to_remote(jencode(reply))
    else:
        account_id = request.form["id"]

        try:
            account_id = int(account_id)
        except:
            reply["log"] = "silly form data provided; parameter <id>"
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
        buyer_names = json_req.get("buyer_names", "bought package") # use packag name if registered member is the one 
                                                                    # buying

        if(IB_id==-1 or type(IB_id)!=type(0)):
            reply["log"] = "silly json data provided; parameter <IB_id>"
            return reply_to_remote(jencode(reply))
        if(amount==-1 and (not (type(amount)==type(0.0) or type(amount)==type(0)))):
            reply["log"] = "silly json data provided; parameter <amount>"
            return reply_to_remote(jencode(reply))

        is_member = 1 if is_member else 0 # convert from True/False to 1/0 (C daint have bools)


    else:
        IB_id = request.form["IB_id"]
        amount = request.form["amount"]
        is_member = request.form["buyer_is_member"]
        buyer_names = request.form["buyer_names"]

        print is_member

        try:
            IB_id = int(IB_id)
            amount = float(amount)
            is_member = 1 if is_member=="true" else 0
        except:
            reply["log"] = "silly form data provided; parameters <IB_id>,<amount> or <buyer_is_member>"
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

    reply["point-factor"] = c_float.in_dll(libghfu, "PF").value
    reply["payment-day"] = c_int.in_dll(libghfu, "PD").value
    reply["account-creation-fee"] = c_float.in_dll(libghfu, "ACF").value
    reply["annual-subscription-fee"] = c_float.in_dll(libghfu, "ASF").value
    reply["operations-fee"] = c_float.in_dll(libghfu, "OF").value
    reply["minimum-investment"] = c_float.in_dll(libghfu, "MinI").value
    reply["maximum-investment"] = c_float.in_dll(libghfu, "MaxI").value

    return reply_to_remote(jencode(reply))

if __name__=="__main__":
    libghfu.init(os.path.join(path,"files","out","server-init")) # ==ALWAYS== INITIATE libghfu before you use it
    init_error = info("server-init")
    if init_error:
        sys.exit(init_error)

    # create contemporary member...to act as first member in case theere are no members yet in structure
    libghfu.register_new_member(0, "PSEUDO-ROOT",
        c_float(c_float.in_dll(libghfu, "ACF").value + c_float.in_dll(libghfu, "ASF").value),
        file_path("pseudo-root"))

    app.run("0.0.0.0", 54321, threaded=1, debug=1)
