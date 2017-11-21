#------------------------------------------------------------------

PASSWORDS = {
    'master':'123456',
}

#------------------------------------------------------------------

PHONEBOOK = {
    'file':'All_clients_Report.xls',
}

#------------------------------------------------------------------

LOGS = {
    'file':'log.log',
    'logall':1,
}

#------------------------------------------------------------------

COUNTRY_CODES = {
    # all country codes MUST start with a '+'

    'Uganda':'+256',
    'Rwanda':'+250',
    'Kenya':'+254',
    'Tanzania':'+255',
    'Sudan':'+249',
    'Burundi':'+257',
}

MINIMUM_WITHDRAW = 3000 # ~ $1

#------------------------------------------------------------------

CHARGES = {
    # if charge<1, the value is a percentage, else, value is an absolute value
    'upload':{
        'YO':.03,
        'mobile-money':0,
        'banks':0 # this fee is not yet clear but when it is, update this ASAP!
    },
    'withdraw':{
        'YO':250,
        'mobile-money':1000, # mtn=390 n airtel=300 but rounded off this figure to this value
        'banks':0 #
    }
}

#------------------------------------------------------------------

try:
    LOG_FILES = [LOGS['file']]
    for LOG_FILE in LOG_FILES:
        if not os.path.isfile(os.path.join(PATH, LOG_FILE)):
            with open(os.path.join(PATH, LOG_FILE), 'w') as f: pass
except:
    import os
    PATH = os.path.dirname(os.path.realpath(__file__))

    LOG_FILES = [LOGS['file']]
    for LOG_FILE in LOG_FILES:
        if not os.path.isfile(os.path.join(PATH, LOG_FILE)):
            with open(os.path.join(PATH, LOG_FILE), 'w') as f: pass

#------------------------------------------------------------------

YO = {
    # API uname and password
    'uname':'100481771305',
    'pswd':'i5nU-rfWr-V7Ip-dEE0-XsAW-Zrra-bbSN-xa4x',

    # YO! account uname and password
    'account-uname':'rkato@jermtechnology.com',
    'account-pswd': 'Richard123#',

    # API communication urls
    'urls':[
            'https://paymentsapi1.yo.co.ug/ybs/task.php',
            'https://paymentsapi2.yo.co.ug/ybs/task2.php',
           ],

    # ***********************
    # methods...
    'methods':{
        'withdraw': 'acwithdrawfunds',
        'transaction_status': 'actransactioncheckstatus',
        'transfer':'acinternaltransfer',
        'transfer_airtime':'acsendairtimeinternal',
        'balance':'acacctbalance',
        'mini_statement':'acgetministatement',
        'airtime':'acsendairtimemobile',
    },

    'currencies':{ # supported YO! currencies
        'mtn-airtime': 'UGX-MTNAT',
        'mtn-mobile-money': 'UGX-MTNMM',
        'airtel-airtime': 'UGX-AIRAT'
    },

    'headers':{
        'Content-Type': 'text/xml',
        'Content-transfer-encoding': 'text',
    },

    # ************************
    # templates, as described in the YO! API documentation
    # format:
    #   (template, parameter_list), where parameter_list is a tuple of 2-item tuples of the format (param, IsMandatory)

    'templates':{
        'withdraw_template':(
    '''<?xml version='1.0' encoding='UTF-8'?>
    <AutoCreate>
        <Request>
            <APIUsername>{}</APIUsername>
            <APIPassword>{}</APIPassword>
            <Method>{}</Method>
            <NonBlocking>{}</NonBlocking>
            <Amount>{}</Amount>
            <Account>{}</Account>
            <AccountProviderCode>{}</AccountProviderCode>
            <Narrative>{}</Narrative>
            <NarrativeFileName>{}</NarrativeFileName>
            <NarrativeFileBase64>{}</NarrativeFileBase64>
            <InternalReference>{}</InternalReference>
            <ExternalReference>{}</ExternalReference>
            <ProviderReferenceText>{}</ProviderReferenceText>
        </Request>
    </AutoCreate>
    ''',
        [
            ('uname',1), ('password',1), ('mtd',1), ('non_blocking',0), ('amount', 1),
            ('number',1), ('account_provider_code', 0), ('narrative',1), ('narrative_file_name',0),
            ('narrative_file_base64',0), ('internal_reference',0), ('external_reference',0), ('provider_reference_text', 0)
            # NB: number is format CC* eg '256783573700'
        ]
       ),

        'deposit_template':( # decide btn 'pull', 'push' n 'instant pay' mthds
    '''<?xml version='1.0' encoding='UTF-8'?>
    <AutoCreate>
        <Request>
            <APIUsername>{}</APIUsername>
            <APIPassword>{}</APIPassword>
            <Method>{}</Method>

            ...

        </Request>
    </AutoCreate>
    ''', 
        [
            ('uname',1), ('password',1), ('mtd',1), 
        ]
       ),

        'transfer_template':(
    '''<?xml version='1.0' encoding='UTF-8'?>
    <AutoCreate>
        <Request>
            <APIUsername>{}</APIUsername>
            <APIPassword>{}</APIPassword>
            <Method>{}</Method>
            <CurrencyCode>{}</CurrencyCode>
            <Amount>{}</Amount>
            <BeneficiaryAccount>{}</BeneficiaryAccount>
            <BeneficiaryEmail>{}</BeneficiaryEmail>
            <Narrative>{}</Narrative>
            <NarrativeFileName>{}</NarrativeFileName>
            <NarrativeFileBase64>{}</NarrativeFileBase64>
            <InternalReference>{}</InternalReference>
            <ExternalReference>{}</ExternalReference>
        </Request>
    </AutoCreate>
    ''', 
        [
            ('uname',1), ('password',1), ('mtd',1), ('currency_code',1), ('amount',1), ('receive_number',1),
            ('receive_email',1), ('narrative',1), ('narrative_file_name',0), ('narrative_file_base64', 0),
            ('internal_reference', 0), ('external_reference',0)
        ]
       ),

        'transfer_airtime_template':(
    '''<?xml version='1.0' encoding='UTF-8'?>
    <AutoCreate>
        <Request>
            <APIUsername>{}</APIUsername>
            <APIPassword>{}</APIPassword>
            <Method>{}</Method>
            <CurrencyCode>{}</CurrencyCode>
            <Amount>{}</Amount>
            <BeneficiaryAccount>{}</BeneficiaryAccount>
            <BeneficiaryEmail>{}</BeneficiaryEmail>
            <Narrative>{}</Narrative>
            <NarrativeFileName>{}</NarrativeFileName>
            <NarrativeFileBase64>{}</NarrativeFileBase64>
            <InternalReference>{}</InternalReference>
            <ExternalReference>{}</ExternalReference>
        </Request>
    </AutoCreate>
    ''', 
        [
            ('uname',1), ('password',1), ('mtd',1), ('currency_code',1), ('amount',1), ('receive_number',1),
            ('receive_email',1), ('narrative',1), ('narrative_file_name',0), ('narrative_file_base64', 0),
            ('internal_reference', 0), ('external_reference',0)
        ]
       ),

        'buy_airtime_template':(
    '''<?xml version='1.0' encoding='UTF-8'?>
    <AutoCreate>
        <Request>
            <APIUsername>{}</APIUsername>
            <APIPassword>{}</APIPassword>
            <Method>{}</Method>
            <NonBlocking>{}</NonBlocking>
            <Amount>{}</Amount>
            <Account>{}</Account>
            <InternetBundle>{}</InternetBundle>
            <AccountProviderCode>{}</AccountProviderCode>
            <Narrative>{}</Narrative>
            <NarrativeFileName>{}</NarrativeFileName>
            <NarrativeFileBase64>{}</NarrativeFileBase64>
            <InternalReference>{}</InternalReference>
            <ExternalReference>{}</ExternalReference>
            <ProviderReferenceText>{}</ProviderReferenceText>
        </Request>
    </AutoCreate>
    ''', 
        [
            ('uname',1), ('password',1), ('mtd',1), ('non_blocking',0), ('amount', 1), ('number',1),
            ('internet_bundle',0), ('account_provider_code',0), ('narrative',1), ('narrative_fileName',0),
            ('narrative_file_base64',0), ('internal_reference',0), ('external_reference',0),
            ('provider_reference_text',0)
        ]
       ),

        'get_balance_template':(
    '''<?xml version='1.0' encoding='UTF-8'?>
    <AutoCreate>
        <Request>
            <APIUsername>{}</APIUsername>
            <APIPassword>{}</APIPassword>
            <Method>{}</Method>
        </Request>
    </AutoCreate>
    ''', 
        [
            ('uname',1), ('password',1), ('mtd',1)
        ]
       ),

        'get_mini_statement_template':(
    '''<?xml version='1.0' encoding='UTF-8'?>
    <AutoCreate>
        <Request>
            <APIUsername>{}</APIUsername>
            <APIPassword>{}</APIPassword>
            <Method>{}</Method>
            <StartDate>{}</StartDate>
            <EndDate>{}</EndDate>
            <TransactionStatus>{}</TransactionStatus>
            <CurrencyCode>{}</CurrencyCode>
            <ResultSetLimit>{}</ResultSetLimit>
            <TransactionEntryDesignation>{}</TransactionEntryDesignation>
            <ExternalReference>{}</ExternalReference>
        </Request>
    </AutoCreate>
    ''', 
        [
            ('uname',1), ('password',1), ('mtd',1), ('start_date',0), ('end_date',0), ('transaction_status',0),
            ('currency_code',0), ('maximum_results',0), ('transaction_entry_designation',0), ('external_reference',0)
        ]
       ),

        'get_transaction_status_template':(
    '''<?xml version='1.0' encoding='UTF-8'?>
    <AutoCreate>
        <Request>
            <APIUsername>{}</APIUsername>
            <APIPassword>{}</APIPassword>
            <Method>{}</Method>
            <TransactionReference>{}</TransactionReference>
            <PrivateTransactionReference>{}</PrivateTransactionReference>
            <DepositTransactionType>{}</DepositTransactionType>        
        </Request>
    </AutoCreate>
    ''', 
        [('uname',1), ('password',1), ('mtd',1), ('reference',1), ('private_reference',1), ('transaction_type',0)]
       ),

    }
}
 
#------------------------------------------------------------------

DATA_BUNDLES = {
    # NB: write script to automatically update this information from 'https://www.mtn.co.ug/internet/Mobile Internet/Pages/'
    #     or let someone regulary update it manually. This is because this information is likely to change rapidly with time
    #     due to funny MTN promotions and all that
    
    
    '1 week':{ # weekly dundles
        '50MB':1800,
        '100MB':3000,
    },
        
    '1 month':{ # monthly bundles
        '25MB':1500, 
        '100MB':4500,
        '500MB':20000,
        '1GB':35000,
        '2GB':50000,
        '3GB':60000, 
        '5GB':90000,
        '10GB':125000, 
        '30GB':285000, 
        '50GB':350000,
        '100GB':500000,
    },

    '3 months':{ # 3 months bundles 
        '1GB':45500, 
        '2GB':65000,
        '3GB':78000,
        '5GB':117000, 
        '10GB':162500, 
        '30GB':370500, 
        '50GB':455000,
        '100GB':650000, 
    },
}
