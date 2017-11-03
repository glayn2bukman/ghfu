import os, sys, json, time
from ctypes import *

root = os.path.realpath(__name__)
root = os.path.split(root)[0]

fout = os.path.join(root,"test","log")
jout = os.path.join(root,"test","json")

libghfu = CDLL(os.path.join(root,"lib","libjermGHFU.so"))

libghfu.init(fout)

if(open(fout,"r").read()): sys.exit(open(fout,"r").read())

root_account_ids = []
fgen_account_ids = []
sgen_account_ids = []

root_accounts = [("root 1",50), ("root 2",50)]
first_gen_accounts = [("fgen 1",1500), ("fgen 2",1500)]
second_gen_accounts = [("sgen 1",1500), ("sgen 2",1500)]

for name, deposit in root_accounts:
    account_id = libghfu.register_new_member(0, name, c_float(deposit), fout)
    if(not account_id):
        print open(fout,"r").read()
        raw_input("press enter to continue...")
    else: root_account_ids.append(account_id)

for i in range(len(root_accounts)):
    account_id = libghfu.register_new_member(root_account_ids[i], first_gen_accounts[i][0], 
        c_float(first_gen_accounts[i][1]), fout)
    if(not account_id):
        print open(fout,"r").read()
        raw_input("press enter to continue...")
    else: fgen_account_ids.append(account_id)

for i in range(len(root_accounts)):
    account_id = libghfu.register_new_member(fgen_account_ids[i], second_gen_accounts[i][0], 
        c_float(second_gen_accounts[i][1]), fout)
    if(not account_id):
        print open(fout,"r").read()
        raw_input("press enter to continue...")
    else: sgen_account_ids.append(account_id)
    
account_ids = root_account_ids + fgen_account_ids + sgen_account_ids

# set payment date to today and start payment
c_int.in_dll(libghfu, "PAYMENT_DAY").value = time.gmtime()[2]

libghfu.perform_monthly_operations(
    ((c_float*2)*4)((375,64), (250,60), (125,49), (0,0)), 
    fout)

# display account information
for account_id in account_ids:
    if libghfu.dump_structure_details(account_id, jout):
        account_data = json.JSONDecoder().decode(open(jout,"r").read())
        print "< ID %d>"%account_id
        for key in account_data: print "    {}: {}".format(key, account_data[key])
        print
    else:
        raw_input("ERROR in dumping account id %d. press enter to continue..."%account_id)

# now dump structure
print "dumped constants?","yes" if libghfu.dump_constants(os.path.join(root,"lib"), os.path.join(root,"test")) else "no"
print "loaded constants?", "yes" if libghfu.load_constants(os.path.join(root,"lib"), os.path.join(root,"test")) else "no"
print "saved structure?", "yes" if libghfu.save_structure(os.path.join(root,"lib"), os.path.join(root,"test")) else "no"
print "loaded constants?", "yes" if libghfu.load_structure(os.path.join(root,"lib"), os.path.join(root,"test")) else "no"
