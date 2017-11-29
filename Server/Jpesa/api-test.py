import requests

url = "https://secure.jpesa.com/api.php"
test_uname = "********"
test_pswd = "*********"

def deposit(number, amount):
    "deposit money from mobile money number to jpesa account"
    
    try:
        print requests.post(url,
            data = {
                "command":"jpesa",
                "action":"deposit",
                "username":test_uname,
                "password":test_pswd,
                "number":str(number),
                "amount":int(amount),
                }
        ).text
    except:
        print "network or url down. if not, you entered an invalid number or amount"

def check_transaction(ref):
    try:
        print requests.post(url,
            data = {
                "command":"jpesa",
                "action":"info",
                "username":test_uname,
                "password":test_pswd,
                "tid":str(ref)
                }        
        ).text
        
    except:
        print "network or url down!"

#deposit("0752567374",500)
check_transaction("0B557AD6334B8A2D075741D1CE055D17")

# references: 
#   airtel:6598863FBA4747BFA647676370AD8780
#   mtn: FECFDF8EF1D12115750425C81D79B9A4
