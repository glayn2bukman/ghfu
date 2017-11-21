import __config__ as cfg, YO_API as YO

from flask import Flask, request, Response
import json, time

jencode = json.JSONEncoder().encode
jdecode = json.JSONDecoder().decode

CODE = "8*d08475u60-=38732nkdhwjjdwdf/-"
CHARGES = ["upload", "withdraw"]


def balance():
    return YO.get_balance()

app = Flask(__name__)

# define a function that wil allow responses to be sent to pages not served by this server
def reply_to_remote(reply):
    response = Response(reply)
    response.headers["Access-Control-Allow-Origin"] = "*" # allow all domains...
    return response

# define scanning function (we ony wanna accept requests from known clients)
def client_known(addr):
    if addr!="127.0.0.1": return 0
    return 1

@app.route("/charges", methods=["POST"])
def charges():
    if not client_known:
        return reply_to_remote(jencode({"status":False, "log":"unknown client"}))

    json_req = request.get_json()
    if not json_req:
        return reply_to_remote(jencode({"status":False, "log":"json data expected in request!"}))

    reply = {"status":False, "log":""}

    code = json_req.get("code","")
    if (not code) or (code!=CODE):
        reply["log"] = "you are not authorized to access this API!"
        return reply_to_remote(jencode(reply))

    charge_type = json_req.get("type","")

    if (not charge_type) or (not (charge_type in CHARGES)):
        reply["log"] = "invalid charge-type requested!. available are <{}>".format(",".join(CHARGES))
        return reply_to_remote(jencode(reply))

    reply = cfg.CHARGES[charge_type]
    
    return reply_to_remote(jencode(reply))
    

@app.route("/withdraw", methods=["POST"])
def withdraw():
    if not client_known:
        return reply_to_remote(jencode({"status":False, "log":"unknown client"}))

    json_req = request.get_json()
    if not json_req:
        return reply_to_remote(jencode({"status":False, "log":"json data expected in request!"}))

    reply = {"status":False, "log":""}
    
    code = json_req.get("code","")
    if (not code) or (code!=CODE):
        reply["log"] = "you are not authorized to access this API!"
        return reply_to_remote(jencode(reply))
    
    try: 
        YO.transactions_db.close()
        YO.db.CONFIG_DB.close()
    except: pass

    YO.db.CONFIG_DB = YO.db.Database(YO.db.dbfile)
    YO.transactions_db = YO.db.Database(YO.os.path.join(YO.PATH, "db", YO.transactions_db_name))

    amount = json_req.get("amount",0) # this amount is independent of the charges
                                      # (those will be calculated by YO)

    if (not amount) or amount<0:
        reply["log"] = "silly amount provided!"
        return reply_to_remote(jencode(reply))  
    if amount<cfg.MINIMUM_WITHDRAW:
        reply["log"] = "amount less than minimum withdrawable amount of <{}>".format(cfg.MINIMUM_WITHDRAW)
        return reply_to_remote(jencode(reply))  

    try:balance = YO.get_balance()
    except:
        reply["log"] = "failed to reach finance system server. is it offline? if not, are we?"
        return reply_to_remote(jencode(reply))  
    
    if balance["Status"]=="ERROR":
        reply["log"] = "failed to communicate with the payments system"
        reply["details"]=balance
        return reply_to_remote(jencode(reply))    

    charges = cfg.CHARGES["withdraw"]
    if amount+charges["mobile-money"]+charges["YO"] > balance["Balance"]['mtn-mobile-money']:
        reply["log"] = "insufficient balance on the system FLOAT"
        return reply_to_remote(jencode(reply))    

    number = json_req.get("number","")
    number = YO.transform_number(number,"Uganda")
    
    if "Error" in number:
        reply["log"] = "invalid phone number given"
        return reply_to_remote(jencode(reply))

    token = json_req.get("token","{} withdrawing {}/= on <{}>".format(number,amount,time.asctime()))

    try:
        _reply = YO.withdraw(number, amount, token)
    except:
        reply["log"] = "failed to reach finance system server. is it offline? if not, are we?"
        return reply_to_remote(jencode(reply))  
    
    reply["status"] = True
    reply["details"] = _reply
    
    return reply_to_remote(jencode(reply))

if __name__== "__main__":
    app.run("0.0.0.0", 54322, threaded=1, debug=1)
