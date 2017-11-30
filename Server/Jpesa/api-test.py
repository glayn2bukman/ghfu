import requests

url = "https://secure.jpesa.com/api.php"

auth_data = open("/var/lib/ghfu/.jpesa").read().strip()

i = auth_data.index(";")
test_uname = auth_data[:i]
test_pswd = auth_data[i+1:]

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

# deposit("0701173049",500)
# check_transaction("9F1A2E5FA6BA4D3E5C81D898E3AA0E55")

# references: 
#   airtel:6598863FBA4747BFA647676370AD8780
#   mtn: FECFDF8EF1D12115750425C81D79B9A4
