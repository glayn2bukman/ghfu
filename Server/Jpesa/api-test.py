import requests

url = "https://secure.jpesa.com/api.php"
test_uname = "glayn2bukman@gmail.com"
test_pswd = "saltySALTY123*"

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
                "amount":int(amount)
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

# deposit("0701173049",5000)
# check_transaction("6598863FBA4747BFA647676370AD8780")

# references: 
#   airtel:6598863FBA4747BFA647676370AD8780
#   mtn: FECFDF8EF1D12115750425C81D79B9A4
