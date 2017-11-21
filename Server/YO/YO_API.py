"""
YO! API as specified in the API documentation

the basic usage scheme of this custom YO! API is:
    1) generate template matching the activity (eg loading data)
    2) send the data to one of the YO! API urls with <requests> using the POST method
        * headers = config.YO["headers"]
        * data = yo_xml_template
        * url = config.YO["urls"][0]

        reply = requests.post(url, data=yo_xml_template, headers=config.YO["headers"])
    3) analyze the reply from YO! for possible error statuses with function "get_xm_values" eg
        if Status=="ERROR":
            ....

        NB: not all Status=ERROR replies have a <Message> parameter, some have <StatusMessage>...
            read the YO! API documentation for details

"""

import requests
import db
PATH = db.path
os = db.os
sys = db.sys

get_data = db.get_data

#print get_data("data_bundles")

# the YO! structure uses xml entirely so we have to define a custom xml parser...
def get_xml_values(yo_xml, *parameters):
    """
    given the xml, y_xml, extract the values of the parameters in <paramaters>

    main parameters of interest are : Status, StatusCode, StatusMessage, ErrorMessage, TransactionReference
    """

    values = {}

    for parameter in parameters:
        tag, rtag = "<{}>".format(str(parameter)), "</{}>".format(str(parameter))
        if not ( tag in yo_xml):
            values[parameter] = None
            continue
        index, rindex = yo_xml.index(tag), yo_xml.index(rtag)
        values[parameter] = yo_xml[index+len(tag):rindex]

    if "<Balance><Currency>" in yo_xml: 
                              # special case; ie when checking for balance. but if absent while 
                              # checking for balance, then NO TRANSACTION HAS BEEN MADE YET!
                              # read the YO! API documentation, pg 39
        currencies = yo_xml.count("<Currency>")
        section = yo_xml[yo_xml.index("<Balance>")+1:]
        values["Balance"] = {}

        while currencies>0:
            ci, rci, bi, rbi = section.index("<Code>"), section.index("</Code>"), section.index("<Balance>"),section.index("</Balance>")
            values["Balance"][section[ci+len("<Code>"):rci]] = float(section[bi+len("<Balance>"):rbi])
            currencies -= 1
            section = section[section.index("</Currency>"):]

    elif "<Transaction>" in yo_xml:
                              # special case for get_mini_statement, read YO! API documentation, pg 41-44        

        transactions = yo_xml.count("<Transaction>"); print transactions
        values["Transactions"]={}
        section = yo_xml[yo_xml.index("<Transaction>")+1:]
        
        floats = ("Amount", "Balance")
    
        while transactions>0:
            tref = "TransactionReference"
            ri, rri =    section.index("<%s>"%tref), section.index("</%s>"%tref)
            
            tstatus = "TransactionStatus"
            si, rsi = section.index("<%s>"%tstatus), section.index("</%s>"%tstatus)
            tstatus = tstatus, si, rsi
            
            idate = "InitiationDate"
            idi, ridi = section.index("<%s>"%idate), section.index("</%s>"%idate)
            idate = idate, idi, ridi
            
            cdate = "CompletionDate"
            cdi, rcdi = section.index("<%s>"%cdate), section.index("</%s>"%cdate)
            cdate = cdate, cdi, rcdi
            
            c = "Currency"
            ci, rci = section.index("<%s>"%c), section.index("</%s>"%c)
            c = c, ci, rci
            
            a = "Amount"
            ai, rai = section.index("<%s>"%a), section.index("</%s>"%a)
            a = a, ai, rai
            
            b = "Balance"
            bi, rbi = section.index("<%s>"%b), section.index("</%s>"%b)
            b = b, bi, rbi

            ref = section[ri+len(tref)+len("<>"):rri]
            values["Transactions"][ref] = {}
            for param in [tstatus, idate, cdate, c, a, b]:
                if param[0] in floats:
                    values["Transactions"][ref][param[0]] = float(section[param[1]+len(param[0])+len("<>"):param[2]])
                else:
                    values["Transactions"][ref][param[0]] = section[param[1]+len(param[0])+len("<>"):param[2]]
                
            
            
            transactions -= 1
            section = section[section.index("</Transaction>"):]


    return values

def send_request(template, **kwargs):
    """
    create YO! xml template:
    arg <template>: template in config.YO
    arg <kwargs>: dict with values of parameters to be given to the template, kwargs left out will be <None>

    example:
        temp = create_yo_xml("withdraw_template", uname="bukman", password="123", mtd="withdraw", narrative="hi",
                    number="256783573700", amount=5600)
    """

    templates = get_data("templates")
    methods = get_data("methods")

    if not template in templates: return "Error: template <{}> is\
 unknown.\nAvailable templates are:\n".format(template) + \
        ("    - <{}>\n"*len(templates)).format(templates.keys())

    

    if not kwargs.get("mtd", ""): return "Error: parameter <mtd> is\
 mandatory. not given!"

    if not (kwargs.get("mtd","") in methods): 
        return "Error: method <{}> is\
 unknown, available YO! methods are:\n".format(kwargs.get("mtd","")) + \
        ("    - <{}>\n"*len(methods)).format(*methods.keys())

    kwargs["mtd"] = methods[kwargs.get("mtd","")]

    parameter_values = templates[template][1][:]
    for index, parameter in enumerate(parameter_values):
        # put the parameters in their ryt place, give "" as default parameter 
        param = kwargs.get(parameter[0], "")

        if (not param) and parameter[1]: return "Error: parameter <{}>\
 is mandatory for <{}>. It wasnt given!".format(parameter[0], template)

        # change from python bool to YO_xml_BOOL
        if parameter=="internet_bundle":
            param = "true" if param==True else param
            param = "true" if param==False else param
        else:
            param = "TRUE" if param==True else param
            param = "FALSE" if param==False else param

        parameter_values[index] = param

    template = templates[template][0].format(*parameter_values)

    urls = get_data("urls")

    for index, url in enumerate(urls):
        print "Sending request to <{}> ...".format(url)
        reply = requests.post(urls[0], data=template, headers=get_data("headers"))
        if reply.status_code==200:
            print "\bRequest sent"
            break

        print "\bFailed"

    if reply.status_code==404:
        return "Error: All the YO! urls in config.YO[\"urls\"] are offline!"

    status = get_xml_values(reply.text, "Status", "StatusMessage", "Message","TransactionStatus", "TransactionReference")
    _status = {}
    for var in status:
        if status[var]: _status[var] = status[var]

    return _status

#reply = send_request("buy_airtime", uname=get_data("general")["uname"], password=get_data("general")["pswd"], mtd="withdraw", narrative="hi",
#            number="256783573700", amount=5600)
#print "Reply:",reply

# ---------------------------------------------------------------
# the YO! API only accepts numbers of the format: CC*** NOT +CC*** or 0***
def transform_number(number, country):
    
    number = str(number)

    country_codes = get_data("country_codes")

    if not country in country_codes:
        return "Error: Country \"{}\" is not in config file!".format(country)

    if (not number) or (not isinstance(number,str)):
        return "Error: destination number is invalid!"

    # validate destination...
    if number[0]=="0": number = country_codes[country][1:]+number[1:]
    number = (number if number[0]!="+" else number[1:])

    if len(number)==12: return number #eg 256783573700
    return "Error: invalid number provided"

# ---------------------------------------------------------------

# transaction-based functions

def withdraw(number, amount, narrative):
    """
    Withdraw funds from Yo! account
    """

    request = send_request("withdraw", mtd="withdraw", uname=get_data("general")["uname"], password=get_data("general")["pswd"], 
                            amount=amount, number=number, narrative=narrative)

    return request
#.    
#print withdraw("0783573700", 500,  "my withdraw...")

def deposit(amount):
    """
    Deposit funds to YO! account
    """

def transfer(amount, to, to_email, narrative="Funds Transfer", currency="mtn-mobile-money", validate_currency=False):
    """
    Transfer funds from YO! account to another YO! account
    """

    currencies = get_data("currencies")

    if not (currency in currencies):
        return "Error: Currency \"{}\" is unknown! Known\
 currencies are:\n".format(currency) + \
        ("    - \"{}\"\n"*len(currencies)).format(*currencies.keys())

    if validate_currency: return currency

    request = send_request("transfer", mtd="transfer", uname=get_data("general")["uname"], password=get_data("general")["pswd"],
                                amount=amount, receive_number=to, narrative=narrative,
                            currency_code=currencies[currency], receive_email=to_email)
    return request
#.
#print transfer(100, "0783573700", "glayn2bukman@gmail.com", "my transfer")

def transfer_airtime(amount, to, to_email, narrative="Airtime Transfer", currency="mtn-airtime"):
    """
    Buy airtime for other YO! account
    """

    currency = transfer("","","", currency=currency,validate_currency=True)
    if "Error: " in currency: return currency

    request = send_request("transfer_airtime", mtd="transfer_airtime", uname=get_data("general")["uname"], password=get_data("general")["pswd"],
                            amount=amount, receive_number=to, narrative=narrative,
                            currency_code=get_data("currencies")[currency], receive_email=to_email)
    return request
#.
#print transfer_airtime(100,"0783573700","glayn2bukman@gmail.com")

def buy_airtime(airtime, number, narrative="Buy Airtime", country="Uganda", internet_bundle=False):
    """
    Buy airtime for number
    """

    number = transform_number(number, country)
    if "Error: " in number: return number

    request = send_request("buy_airtime", mtd="airtime", uname=get_data("general")["uname"], password=get_data("general")["pswd"], 
                            amount=airtime, number=number, narrative=narrative, internet_bundle=internet_bundle)

    return request

#.
#print buy_airtime(100, "0783573700", "my airtime buying...")


# non transaction-based
def get_transaction_status(reference_number, private_reference):
    """
    Get the transaction status of an action by its reference number
    """

    request = send_request("get_transaction_status", mtd="transaction_status", uname=get_data("general")["uname"], 
                            password=get_data("general")["pswd"], 
                            reference=reference_number, private_reference=private_reference)

    return request
#.
#print get_transaction_status("0658973126485", "0689523")

def get_balance():
    """
    Get YO! account balance
    """

    # note tht even though this function calls "send_request", it should have a different return dictionary
    # as for example the returned xml will have diffent balances for the different currencies. read the YO!
    # payments API (pg 39) for a detailed explanation
     
    request = send_request("get_balance", mtd="balance", 
        uname=get_data("general")["uname"], 
        password=get_data("general")["pswd"])

    return request
#.
#print get_balance()

def get_mini_statement(date_from=None, date_to=None, currency="all"):
    """
    Get YO! account mini statement. Statement will entail activity from <date_from>
    to <date_to>. If <date_from> is None, the last 5 transactions wil b returned!

    NB: date format: YYYY-MM-DD HH:MM:SS eg "2017-04-03 16:23:08"
    """
    
    currency = None if currency=="all" else currency

    if currency: # then set currency in mini statement template...
        currency = transfer("","","", currency,True)
        if "Error: " in currency: return currency
        
    kwargs = {"mtd":"mini_statement", "uname":get_data("general")["uname"], "password":get_data("general")["pswd"]}
    if currency: kwargs["currency_code"] = currency
    if date_from:
        kwargs["start_date"] = date_from
        kwargs["end_date"] = date_to

    request = send_request("get_mini_statement", **kwargs)
    
    return request
#.
#print get_mini_statement()

# ----------------------------------------- DB ---------------------------------------------------

def initiate_DB():
    # create DB directory if absent...
    if not os.path.isdir(os.path.join(PATH, "db")):
        os.mkdir(os.path.join(PATH, "db"))

    tables = {"Transactions":[  
                ("reference", "varchar(15)"), ("transcation", "varchar(25)"), ("date_time","varchar(30)"),
                ("status", "varchar(10)"), ("amount", "float(2)"), ("account","varchar(15)"), ("email", "varchar(50)")
                ],
             }

    global transactions_db
    
    try: transactions_db = db.Database(os.path.join(PATH, "db", transactions_db_name), tables)
    except:
        print "failed to create transaction db...re-run this YO_API script please"
        sys.exit(1)

# initiate the db right away...
transactions_db_name = "Transactions.db"
initiate_DB()

# print [field[0] for field in transactions_db.fields]
# print transactions_db.add(("03568792","withdraw","Sat May  6 11:14:50 2017", "OK", 2500, "256783573700",""))

# print transactions_db.get_all()
